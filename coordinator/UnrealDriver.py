from re import RegexFlag
from Git import CheckoutException
from Git import CloneException
import argparse
import json
import logging
import os
import re
import shutil
from os import PathLike
from pathlib import Path
from typing import override, Any, cast, Final

from DriverBase import DriverBase
from Util import ExecuteOutputOptions, execute, log_exc

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

        self.public_dependency_module_names = self.args.public_dependency_module_names
        self.headers: list[str] = self.args.headers
        self.ubt_platform: str = self.args.ubt_platform
        self.ubt_config: str = self.args.ubt_config
        self.broken_branches: Final[dict[str, Path]] = _get_broken_branches()
        self.__checkout_ran = False

    @override
    def make_compile_commands(self, branch: str) -> Path:
        self.project_generator = _make_generator(branch, self)
        if self.project_generator is None:
            log_exc(f"Failed to generate uproject for branch {branch}!")
        self.__checkout_ran = True
        self.project_generator.run_setup()
        self.project_generator.write_project_files()
        self.project_generator.run_generate_project_files()
        self.project_generator.run_ubt()
        self.project_generator.run_generate_clang_database() #todo may not need this if we can find the same files present in ue4 builds
        compile_commands = self.git.root / "compile_commands.json"
        if not compile_commands.exists():
            log_exc(f"Failed to find generated compile_commands.json for branch {branch}!")
        return compile_commands

    @override
    def get_target_cpp(self) -> Path:
        if not self.project_generator:
            log_exc("get_target_cpp should only be called after make_compile_commands!")
        return self.project_generator.project_src / "MetadataAnalysis.cpp"

    @override
    def with_argument_parser(self, parser: argparse.ArgumentParser):
        parser.add_argument("--public-dependency-module-names", nargs='+', type=str, default=["Core", "CoreUObject", "Engine"],
                            help="The names of the Unreal modules to include. Defaults are \"Core\", \"CoreUObject\", and "
                                 "\"Engine\". If you override this, the defaults will be erased.")
        parser.add_argument("--headers", nargs='+', type=str, required=True,
                            help="Unreal .h files to include in the analysis.")
        parser.add_argument("--ubt-platform", type=str, default="Win64",
                            help="Unreal platform to use. Defaults to \"Win64\"")
        parser.add_argument("--ubt-config", type=str, default="Shipping",
                            help="Unreal configuration to use. Defaults to \"Shipping\".")

    @override
    def on_clone_exception(self, ex: CloneException) -> None:
        if not self.__handle_git_ex(ex.branch):
            super().on_clone_exception(ex)


    @override
    def on_checkout_exception(self, ex: CheckoutException) -> None:
        if not self.__handle_git_ex(ex.branch):
            super().on_checkout_exception(ex)

    @override
    def on_before_next_checkout(self, branch: str):
        super().on_before_next_checkout(branch)
        self.__checkout_ran = False

    def __handle_git_ex(self, branch: str) -> bool:
        if self.__checkout_ran:
            return False

        file = self.broken_branches[branch]
        if file is None:
            return False

        target = self.git.root / "Engine" / "Build" / "Commit.gitdeps.xml"
        if not target.exists():
            logging.error(f"Failed to handle bad branch {branch}!")
            return False

        shutil.copy2(file, target)
        return True


def _make_generator(branch: str, driver: UnrealDriver) -> DefaultUnrealProjectGenerator:
    canonical = _canonical_branch(branch)
    def gen_helper(cls: type[DefaultUnrealProjectGenerator]) -> DefaultUnrealProjectGenerator | None:
        subclasses: list[type[DefaultUnrealProjectGenerator]] = cls.__subclasses__()
        for subclass in subclasses:
            if canonical in subclass.valid_for:
                return subclass(branch, driver)
            else:
                child_gen = gen_helper(subclass)
                if child_gen is not None:
                    return child_gen
        return None
    gen = gen_helper(DefaultUnrealProjectGenerator)
    return gen if gen is not None else DefaultUnrealProjectGenerator(branch, driver)

def _get_broken_branches():
    directory = Path.cwd() / "external"
    if not directory.exists() or not directory.is_dir():
        raise Exception(f"Could not construct BrokenBranchesDict because directory {directory} does not exist!")

    bbr = re.compile(r".*Commit\.gitdeps\.(?P<branch>(?P<major>\d+\.\d+)(\.(?P<patch>\d+))?).xml$",
                     RegexFlag.M | RegexFlag.U)
    out: dict[str, Path] = {}
    for file in directory.iterdir():
        match = bbr.match(file.name)
        if match is not None:
            groups = match.groupdict()
            if groups["patch"] is None or groups["patch"] == "0":
                out[groups["major"]] = file
            out[groups["branch"]] = file
            out[groups["branch"] + "-release"] = file

    return out

CANONICAL_BRANCH_RE = re.compile(r"^(?P<version>(?P<major>\d+\.\d+)(\.(?P<patch>\d+))?)(-release)?$",
                                 RegexFlag.M | RegexFlag.U)
def _canonical_branch(branch: str):
    match = CANONICAL_BRANCH_RE.match(branch)
    if match is None:
        if branch.startswith("dev"):
            raise Exception(f"Invalid branch '{branch}': development branches not allowed!")
        raise Exception(f"Invalid branch '{branch}': canonical branches ('{{version}}' or '{{version}}-release') only!")

    match_dict = match.groupdict()
    patch = match_dict['patch']
    if patch is None or patch == "0":
        return match_dict["major"]
    return match_dict["version"]

class DefaultUnrealProjectGenerator:
    valid_for: set[str] = set()
    def __init__(self, branch: str, driver: UnrealDriver):
        super().__init__()
        self.branch = branch
        self.driver = driver
        self.git = self.driver.git
        self.setup_path = self.git.root / f"Setup.{self.driver.platform_shell_ext}"
        self.generate_project_files_path = self.git.root / f"GenerateProjectFiles.{self.driver.platform_shell_ext}"
        self.project_path = self.driver.test_project_path / "MetadataHarness.uproject"
        self.project_root = self.driver.test_project_path
        self.project_src = self.project_root / "Source" / "MetadataHarness"
        self.dotnet_path: Path | None = None
        self.ubt_path = self.get_ubt_path()

    def run_setup(self):
        if not self.setup_path.exists():
            log_exc(f"Failed to find Setup.bat file in Unreal branch {self.branch}!")
        execute(self.setup_path,
                success_msg=f"Setup.bat completed for branch {self.branch}!",
                fail_msg=f"Failed to run Setup.bat in Unreal branch {self.branch}!",
                output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                addl_env=self.get_env())

    def get_ubt_path(self):
        return self.git.root / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.dll"

    def get_generate_project_files_args(self) -> list[PathLike[str] | str]:
        generate_project_files_args: list[PathLike[str] | str] = [self.generate_project_files_path, f"-project={self.project_path}"]
        if self.driver.platform == "win32":
            generate_project_files_args.append("-game")
            generate_project_files_args.append("-engine")
        return generate_project_files_args

    def run_generate_project_files(self):
        if not self.generate_project_files_path.exists():
            log_exc(f"Failed to find GenerateProjectFiles for UnrealEngine branch {self.branch}!")

        execute(self.get_generate_project_files_args(),
                success_msg=f"Successfully ran GenerateProjectFiles script for UnrealEngine branch {self.branch}.",
                fail_msg=f"Failed to run GenerateProjectFiles script for UnrealEngine branch {self.branch}!",
                output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                addl_env=self.get_env())

    def get_mh_target_cs(self) -> str:
        return """
                    using UnrealBuildTool;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            DefaultBuildSettings = BuildSettingsVersion.Latest;
                            IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
                        }
                    }
                """

    def get_mh_build_cs(self, module_names: str):
        return f"""
            using UnrealBuildTool;
            public class MetadataHarness : ModuleRules
            {{
                public MetadataHarness(ReadOnlyTargetRules Target) : base(Target)
                {{
                    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

                    PublicDependencyModuleNames.AddRange(new[]
                    {{
                        {module_names}
                    }});
                }}
            }}
        """

    def get_mh_h(self):
        return """
                #pragma once
                #include "CoreMinimal.h"
            """

    def get_mh_cpp(self, includes: str):
        return f"""
            #include "MetadataHarness.h"
            {includes}
        """

    def write_project_files(self):
        if self.project_root.exists():
            shutil.rmtree(self.project_root)
        self.project_src.mkdir(parents=True, exist_ok=True)

        with open(self.project_path, "w", encoding="utf-8") as uproject_file:
            json.dump(self.get_uproject(), uproject_file)

        module_names = ", ".join(json.dumps(module) for module in self.driver.public_dependency_module_names)
        with open(self.project_root / "Source" / "MetadataHarness.Target.cs", "w", encoding="utf-8") as target_cs_file:
            target_cs_file.write(self.get_mh_target_cs())

        with open(self.project_src / "MetadataHarness.Build.cs", "w", encoding="utf-8") as build_cs_file:
            build_cs_file.write(self.get_mh_build_cs(module_names))

        with open(self.project_src / "MetadataHarness.h", "w", encoding="utf-8") as harness_header_file:
            harness_header_file.write(self.get_mh_h())
        with open(self.project_src / "MetadataHarness.cpp", "w", encoding="utf-8") as harness_src_file:
            harness_src_file.write("""
                #include "MetadataHarness.h"
                IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MetadataHarness, "MetadataHarness");
            """)
        with open(self.project_src / "MetadataAnalysis.cpp", "w", encoding="utf-8") as anal_src_file:
            includes = "".join(f"#include \"{header.replace("\\", "/")}\"\n" for header in self.driver.headers)
            anal_src_file.write(self.get_mh_cpp(includes))

    def get_bundled_dotnet(self, platform_dict: dict[str, str] | None = None) -> Path:
        if platform_dict is None:
            platform_dict = {"win32": "win-x64", 'linux': 'linux-x64', 'darwin': 'mac-x64'}
        dotnet_root = self.git.root / "Engine" / "Binaries" / "ThirdParty" / "DotNet"
        if not dotnet_root.exists():
            log_exc("Failed to find ThirdParty/DotNet for UnrealEngine.")

        dotnet_exe = f"dotnet{'.exe' if self.driver.platform == 'win32' else ''}"

        def entry_is_dotnet(entry: Path):
            return (entry.is_dir()
                    and re.search(r"^(\d+\.)+\d+$", entry.name, re.RegexFlag.M)
                    and (entry / platform_dict[self.driver.platform] / dotnet_exe).exists())

        def dotnet_version(entry: Path) -> tuple[int, ...]:
            return tuple(int(part) for part in entry.name.split("."))

        bundled_dotnet_versions: list[Path] = [entry for entry in dotnet_root.iterdir() if entry_is_dotnet(entry)]
        bundled_dotnet_versions.sort(key=dotnet_version, reverse=True)
        if len(bundled_dotnet_versions) == 0:
            log_exc("Failed to find bundled DotNet for UnrealEngine.")
        return (bundled_dotnet_versions[0] / platform_dict[self.driver.platform] / dotnet_exe).resolve()

    def get_ubt_args(self, working_dir: Path) -> list[PathLike[str] | str]:
        return [cast(Path, self.dotnet_path), self.ubt_path, "MetadataHarness", self.driver.ubt_platform, self.driver.ubt_config,
         f"-project={self.project_path}",
         "-WaitMutex", "-architecture=x64",
         f"-WorkingDir={working_dir}",
         f"-Files={self.project_src / 'MetadataAnalysis.cpp'}"]

    def run_ubt(self):
        if not self.ubt_path.exists():
            log_exc(f"Failed to find UnrealBuildTool for UnrealEngine branch {self.branch}!")
        if self.dotnet_path is None:
            self.dotnet_path = self.get_bundled_dotnet()
        working_dir = self.project_root / "Intermediate" / "ProjectFiles"
        execute(self.get_ubt_args(working_dir),
                success_msg=f"Successfully ran UBT/UHT for branch {self.branch}.",
                fail_msg=f"Failed to run UBT/UHT for branch {self.branch}.",
                output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                addl_env=self.get_env())

    def get_generate_clang_database_args(self) -> list[PathLike[str] | str]:
        return [self.generate_project_files_path, "-Mode=GenerateClangDatabase", "MetadataHarness",
                self.driver.ubt_platform, self.driver.ubt_config, f"-project={self.project_path}"]

    def run_generate_clang_database(self):
        execute(self.get_generate_clang_database_args(),
                success_msg=f"Successfully ran GenerateClangDatabase for branch {self.branch}.",
                fail_msg=f"Failed to run GenerateClangDatabase for branch {self.branch}!",
                output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                addl_env=self.get_env())

    def get_uproject(self) -> dict[str, Any]:
        pure_version = "".join(re.sub(r'[a-zA-Z]', ' ', self.branch).split())
        return {
            "FileVersion": 3,
            "EngineAssociation": f"{pure_version}",
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

    def get_env(self):
        return {
            "MSBUILDDISABLENODEREUSE": "1",
            "UseSharedCompilation": "false"
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

    _, jsn = execute([vswhere.resolve(), "-utf8", "-format", "json", "-nocolor"],
                     success_msg="Acquired VS version info...",
                     fail_msg="Failed to get VS version info!",
                     expected_ret=0,
                     output=ExecuteOutputOptions.SILENT)

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
    logging.info("Validated VS 2022 install!")

class UPG_54(DefaultUnrealProjectGenerator):
    valid_for = {'5.4', '5.3'}

    def get_bundled_dotnet(self, platform_dict: dict[str, str] | None = None) -> Path:
        return super().get_bundled_dotnet({'linux': 'linux', 'darwin': 'mac-x64', 'win32': 'windows'} if platform_dict is None else platform_dict)

class UPG_52(UPG_54):
    valid_for = {'5.2', '5.1'}

    def get_ubt_args(self, working_dir: Path):
        args = super().get_ubt_args(working_dir)
        for i in range(len(args) - 1, -1, -1):
            if str(args[i]).startswith('-Files'):
                args[i] = f"-SingleFile={self.project_src / 'MetadataAnalysis.cpp'}"
                break

        args.append("-CompilerVersion=14.29.30159")
        args.append("-Compiler=VisualStudio2022")
        return args

class UPG_5(UPG_52):
    valid_for = {'5.0'}
    # missing macro for the current version of the compiler
    _win10_ge_definition = "NTDDI_WIN10_GE=0x0A000010"
    _parser_additional_commands = [
        "--additional-clang-args=/std:c++17",
        "--additional-clang-args=/clang:-Wno-c++11-narrowing"
    ]

    def get_mh_target_cs(self):
        return """
                    using UnrealBuildTool;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            DefaultBuildSettings = BuildSettingsVersion.Latest;
                            if (Target.Platform == UnrealTargetPlatform.Win64)
                            {
                                GlobalDefinitions.Add("__WIN10_GE_DEFINITION__");
                            }
                        }
                    }
                """.replace("__WIN10_GE_DEFINITION__", self._win10_ge_definition)

    def get_bundled_dotnet(self, platform_dict: dict[str, str] | None = None) -> Path:
        if platform_dict is not None:
            return super().get_bundled_dotnet(platform_dict)
        platform_dict = {'linux': 'Linux', 'darwin': 'Mac', 'win32': 'Windows'}
        return self.git.root / "Engine" / "Binaries" / "ThirdParty" / "DotNet" / platform_dict[self.driver.platform] / "dotnet.exe"

    def run_ubt(self):
        if self.driver.platform != "win32":
            super().run_ubt()
            return

        # env workaround for ubt issue
        msvc_define = f"/D{self._win10_ge_definition}"
        previous_cl = os.environ.get("CL")
        if previous_cl is None:
            os.environ["CL"] = msvc_define
        elif msvc_define.lower() not in previous_cl.lower():
            os.environ["CL"] = f"{previous_cl} {msvc_define}"

        try:
            super().run_ubt()
        finally:
            if previous_cl is None:
                os.environ.pop("CL", None)
            else:
                os.environ["CL"] = previous_cl

    def run_generate_clang_database(self):
        super().run_generate_clang_database()
        if self.driver.platform != "win32":
            return

        self.add_parser_additional_commands()

    def add_parser_additional_commands(self):
        for arg in self._parser_additional_commands:
            if arg not in self.driver.parser_additional_commands:
                self.driver.parser_additional_commands.append(arg)

class UPG_427(UPG_5):
    valid_for = {'4.27', '4.26'}

    @override
    def get_ubt_path(self):
        return self.git.root / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool.exe"

    @override
    def get_ubt_args(self, working_dir: Path):
        def_args = super().get_ubt_args(working_dir)
        def_args.pop(0)
        return def_args

    @override
    def run_generate_clang_database(self):
        if self.driver.platform != "win32":
            super().run_generate_clang_database()
            return

        response_file = self.find_metadata_analysis_response_file()
        response_args = [line.strip() for line in response_file.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(response_args) == 0:
            log_exc(f"Failed to synthesize compile_commands.json: {response_file} is empty!")

        compile_commands = [
            {
                "file": str((self.project_src / "MetadataAnalysis.cpp").resolve()),
                "command": " ".join(["clang-cl.exe", *response_args]),
                "directory": str((self.git.root / "Engine" / "Source").resolve())
            }
        ]
        compile_commands_path = self.git.root / "compile_commands.json"
        with open(compile_commands_path, "w", encoding="utf-8") as compile_commands_file:
            json.dump(compile_commands, compile_commands_file, indent=2)
            compile_commands_file.write("\n")

        self.add_parser_additional_commands()
        logging.info(f"Synthesized compile_commands.json for branch {self.branch} from {response_file}.")

    def find_metadata_analysis_response_file(self) -> Path:
        build_root = self.project_root / "Intermediate" / "Build" / self.driver.ubt_platform
        if not build_root.exists():
            log_exc(f"Failed to find build output directory for branch {self.branch}: {build_root}")

        candidates = list(build_root.rglob("MetadataAnalysis.cpp.obj.response"))
        if len(candidates) == 0:
            log_exc(f"Failed to find MetadataAnalysis.cpp response file under {build_root}")

        candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)
        return candidates[0]
