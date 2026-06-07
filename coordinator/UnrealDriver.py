import argparse
import json
import os
import re
from pathlib import Path
from typing import override, Any

from DriverBase import DriverBase
from Util import log_exc, exec_proc


class UnrealDriver(DriverBase):
    def __init__(self):
        super().__init__()
        self.project_generator: DefaultUnrealProjectGenerator | None = None
        match self.platform:
            case "linux":
                self.platform_shell_ext = "sh"
            case "darwin":
                self.platform_shell_ext = "command"
            case "win32":
                self.platform_shell_ext = "bat"
                _validate_msvc()
            case _:
                raise Exception(f"Unsupported platform: {self.platform}")

        self.public_dependency_module_names = self.args.public_dependency_module_names \
            if self.args.public_dependency_module_names else ["Core", "CoreUObject", "Engine"]
        self.headers: list[str] = self.args.headers

    @override
    def make_compile_commands(self, branch: str) -> Path:
        self.project_generator = _make_generator(branch, self)
        if self.project_generator is None:
            log_exc(f"Failed to generate uproject for branch {branch}!")
        self.project_generator.run_setup()
        self.project_generator.run_generate_project_files()
        self.project_generator.run_ubt()
        self.project_generator.run_generate_clang_database()
        return self.repo_root / "compile_commands.json"

    @override
    def get_target_cpp(self) -> Path:
        if not self.project_generator:
            log_exc("get_target_cpp should only be called after make_compile_commands!")
        return self.project_generator.project_src / "MetadataHarness.cpp"

    @override
    def with_argument_parser(self, parser: argparse.ArgumentParser):
        parser.add_argument("--public-dependency-module-names", nargs='+', type=str,
                            help="The names of the Unreal modules to include. Defaults are \"Core\", \"CoreUObject\", and "
                                 "\"Engine\". If you override this, the defaults will be erased.")
        parser.add_argument("--headers", nargs='+', type=str, required=True,
                            help="Unreal .h files to include in the analysis.")

def _make_generator(branch: str, driver: UnrealDriver) -> DefaultUnrealProjectGenerator:
    version_ext = branch.replace(".", "_").replace("-", "_")
    subclasses: list[type[DefaultUnrealProjectGenerator]] = DefaultUnrealProjectGenerator.__subclasses__()

    for subclass in subclasses:
        if subclass.__name__.endswith(version_ext):
            return subclass(branch, driver)
    return DefaultUnrealProjectGenerator(branch, driver)


class DefaultUnrealProjectGenerator:
    def __init__(self, branch: str, driver: UnrealDriver):
        super().__init__()
        self.branch = branch
        self.driver = driver
        self.setup_path = self.driver.repo_root / f"Setup.{self.driver.platform_shell_ext}"
        self.generate_project_files_path = self.driver.repo_root / f"GenerateProjectFiles.{self.driver.platform_shell_ext}"
        self.project_path = self.driver.intermediate_path / "MetadataHarness" / "MetadataHarness.uproject"
        self.project_root = self.driver.intermediate_path / "MetadataHarness"
        self.project_src = self.project_root / "Source" / "MetadataHarness"
        self.dotnet_path = self.get_bundled_dotnet()
        self.ubt_path = self.driver.repo_root / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.dll"

    def run_setup(self):
        if not self.setup_path.exists():
            log_exc(f"Failed to find Setup.bat file in Unreal branch {self.branch}!")
        exec_proc(self.setup_path,
                  f"Setup.bat completed for branch {self.branch}!",
                  f"Failed to run Setup.bat in Unreal branch {self.branch}!")

    def run_generate_project_files(self):
        if not self.generate_project_files_path.exists():
            log_exc(f"Failed to find GenerateProjectFiles for UnrealEngine branch {self.branch}!")

        generate_project_files_args = [self.generate_project_files_path, f"-project=\"{self.project_path}\""]
        if self.driver.platform == "Windows":
            generate_project_files_args.append("-game")
            generate_project_files_args.append("-engine")

        exec_proc(generate_project_files_args,
                  f"Successfully ran GenerateProjectFiles script for UnrealEngine branch {self.branch}.",
                  f"Failed to run GenerateProjectFiles script for UnrealEngine branch {self.branch}!")

    def write_project_files(self):
        if self.project_root.exists():
            self.project_root.rmdir()
        self.project_src.mkdir(parents=True, exist_ok=True)

        with open(self.project_path, "w", encoding="utf-8") as uproject_file:
            json.dump(self.get_uproject(), uproject_file)

        with open(self.project_src / "MetadataHarness.build.cs", "w", encoding="utf-8") as build_cs_file:
            build_cs_file.write(f"""
                using UnrealBuildTool;
                public class MetadataHarness : ModuleRules
                {{
                    public MetadataHarness(ReadOnlyTargetRules Target) : base(Target)
                    {{
                        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

                        PublicDependencyModuleNames.AddRange(new[]
                        {{
                            {", ".join(self.driver.public_dependency_module_names)}
                        }});
                    }}
                }}
            """)

        with open(self.project_src / "MetadataHarness.h", "w", encoding="utf-8") as harness_header_file:
            harness_header_file.write("""
                #pragma once
                #include "CoreMinimal.h"
            """)
        with open(self.project_src / "MetadataHarness.cpp", "w", encoding="utf-8") as harness_src_file:
            harness_src_file.write("""
                #include "MetadataHarness.h"
                IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MetadataHarness, "MetadataHarness");
            """)
        with open(self.project_src / "MetadataAnalysis.cpp", "w", encoding="utf-8") as anal_src_file:
            as_includes = {f"#include \"{header}\"\n" for header in self.driver.headers}
            anal_src_file.write(f"""
                {as_includes}
            """)

    def get_bundled_dotnet(self) -> Path:
        bundled_dotnet = self.driver.repo_root / "Engine" / "Binaries" / "ThirdParty" / "DotNet"
        if not bundled_dotnet.exists():
            log_exc("Failed to find ThirdParty/DotNet for UnrealEngine.")

        def entry_is_dotnet(entry: Path):
            return (entry.is_dir()
                    and re.search(r"^(\d+\.)+\d+$", entry.name, re.RegexFlag.M)
                    and (entry / platform_bundled_dotnet_folder() / f"dotnet{'.exe' if  self.driver.platform == "win32" else ''}").exists())

        def platform_bundled_dotnet_folder():
            match self.driver.platform:
                case "win32":
                    return "win-x64"
                case "linux":
                    return "linux-x64"
                case "darwin":
                    return "mac-x64"
                case _:
                    raise Exception(
                        f"Unrecognized platform when trying to get bundled dotnet folder name: {self.driver.platform}")

        bundled_dotnet: list[Path] = [entry for entry in bundled_dotnet.iterdir() if entry_is_dotnet(entry)]
        bundled_dotnet.sort(reverse=True)
        if len(bundled_dotnet) == 0:
            log_exc("Failed to find bundled DotNet for UnrealEngine.")
        return (bundled_dotnet[0] / platform_bundled_dotnet_folder() / f"dotnet{'.exe' if self.driver.platform == "win32" else ''}").resolve()

    def run_ubt(self):
        if not self.ubt_path.exists():
            log_exc(f"Failed to find UnrealBuildTool for UnrealEngine branch {self.branch}!")
        ubt_args = [self.dotnet_path, f"\"{self.ubt_path}\"", "MetadataHarness", "Win64", "Shipping",
                    f"-project=\"{self.project_path}\"",
                    "-WaitMutex", "-architecture=x64",
                    f"-WorkingDir=\"{self.project_root / "Intermediate" / "ProjectFiles"}\"",
                    f"-Files=\"{self.project_src / "MetadataAnalysis.cpp"}\""]
        exec_proc(ubt_args,
                  f"Successfully ran UBT/UHT for branch {self.branch}.",
                  f"Failed to run UBT/UHT for branch {self.branch}.")


    def run_generate_clang_database(self):
        args = [self.generate_project_files_path, "-Mode=GenerateClangDatabase", "MetadataHarness",
                                   "Win64", "Shipping", f"-project=\"{self.project_path}\""]
        exec_proc(args, f"Successfully ran GenerateClangDatabase for branch {self.branch}.",
                  f"Failed to run GenerateClangDatabase for branch {self.branch}!")

    def get_uproject(self) -> dict[str, Any]:
        return {
            "FileVersion": 3,
            "EngineAssociation": "",
            "Category": "",
            "Description": "",
            "Modules": [
                {
                    "Name": "MetadataHarness",
                    "Type": "Runtime",
                    "LoadingPhase": "Default"
                }
            ]
        }

def _validate_msvc():
    vswhere = os.environ.get("ProgramFiles(x86)")
    if not vswhere:
        raise Exception("ProgramFiles(x86) not found!")
    vswhere = Path(vswhere)
    if not vswhere.exists():
        raise Exception("ProgramFiles(x86) not found!")
    vswhere = vswhere / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        raise Exception("vswhere.exe not found! Is Visual Studio 2022 installed?"
                        "If not, use the unreal editor; download an engine version in the epic launcher,"
                        "create a new project, set it to C++, and it'll give you a valid download.")

    jsn = ""
    exec_proc([vswhere.resolve(), "-utf8", "-json", "-nocolor"],
              "Acquired VS version info...",
              "Failed to get VS version info!", jsn)

    if len(jsn) == 0:
        log_exc("vswhere.exe failed to dump json!")

    try:
        jsn_dict: list[dict[str, str]] = json.loads(jsn)
    except Exception as e:
        log_exc(f"Failed to parse json from vswhere: {e}")

    if len(jsn_dict) == 0:
        log_exc("vswhere.exe failed to dump json!")
    if not any(re.search(r"Visual Studio (Community|Professional|Enterprise) 2022", install['displayName']) for install in jsn_dict):
        log_exc("Visual Studio 2022 install not found, which is required for Unreal Engine! To install it, "
                        "download an engine version in the epic launcher, "
                        "create a new project, set it to C++, and it'll give you a valid download.")