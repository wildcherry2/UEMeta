import urllib.error
import urllib.request
import zipfile
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

_VS2013_ENV: dict[str, str] | None = None
_VS2015_ENV: dict[str, str] | None = None

# note: long paths can be an issue
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
        self.vs2013_vcvarsall: Path | None = self.args.vs2013_vcvarsall.resolve() if self.args.vs2013_vcvarsall else None
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
        self.project_generator.patch_src()
        self.project_generator.run_generate_project_files()
        self.project_generator.write_build_config()
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
        parser.add_argument("--vs2013-vcvarsall", type=Path,
                            help="Path to the VS2013 vcvarsall.bat, or to a VS2013 VC directory containing it. "
                                 "Use this when the standalone 2013 tools did not register the normal VS keys.")

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

        if branch not in self.broken_branches:
            logging.error(f"Failed to handle bad branch {branch}!")
            return False

        file = self.broken_branches[branch]
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

    def patch_src(self):
        pass

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

    def get_mh_build_cs(self, module_names: str)->str: #todo remove pch usage
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

    def get_mh_h(self)->str:
        return """
                #pragma once
                #include "CoreMinimal.h"
            """

    def get_mh_cpp(self, includes: str)->str:
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

    def get_build_config(self) -> str | None:
        return None

    def get_build_config_path(self):
        return self.git.root / "Engine" / "Saved" / "UnrealBuildTool" / "BuildConfiguration.xml"

    def write_build_config(self):
        config_str = self.get_build_config()
        if config_str is None:
            return

        config_path = self.get_build_config_path()
        if not config_path.exists():
            raise Exception(f"Failed to find BuildConfiguration.xml file for branch {self.branch}: {config_path}")
        with open(config_path, "w", encoding="utf-8") as config_file:
            config_file.write(config_str.strip())

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

def _path_value(path: Path) -> str:
    value = str(path)
    return value if value.endswith(("\\", "/")) else value + os.sep

def _env_get(env: dict[str, str], key: str) -> str | None:
    for env_key, value in env.items():
        if env_key.upper() == key.upper():
            return value
    return None

def _env_set(env: dict[str, str], key: str, value: str) -> None:
    existing_key = next((env_key for env_key in env if env_key.upper() == key.upper()), None)
    if existing_key is not None and existing_key != key:
        env.pop(existing_key)
    env[key] = value

def _path_from_value(value: Path | str | None) -> Path | None:
    if value is None:
        return None
    if isinstance(value, Path):
        return value
    value = value.strip().strip('"')
    return Path(value) if value else None

def _is_installed_product(display_name: str) -> bool:
    try:
        import winreg
    except ImportError:
        return False

    display_name_lower = display_name.lower()
    uninstall_key = r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        for access in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY, 0):
            try:
                with winreg.OpenKey(hive, uninstall_key, 0, winreg.KEY_READ | access) as key:
                    for index in range(winreg.QueryInfoKey(key)[0]):
                        try:
                            with winreg.OpenKey(key, winreg.EnumKey(key, index)) as sub_key:
                                value, _ = winreg.QueryValueEx(sub_key, "DisplayName")
                        except OSError:
                            continue

                        if display_name_lower in str(value).lower():
                            return True
            except OSError:
                pass
    return False

def _get_vs_vcvarsall(version: str, override: Path | None = None) -> Path:
    labels = {"12.0": "VS2013", "14.0": "VS2015"}
    common_tools_vars = {"12.0": "VS120COMNTOOLS", "14.0": "VS140COMNTOOLS"}
    override_vars = {
        "12.0": ("UEMETA_VS2013_VCVARSALL", "VS2013_VCVARSALL", "VS120VCVARSALL"),
        "14.0": ("UEMETA_VS2015_VCVARSALL", "VS2015_VCVARSALL", "VS140VCVARSALL"),
    }
    override_args = {"12.0": "--vs2013-vcvarsall", "14.0": "UEMETA_VS2015_VCVARSALL"}
    script_names = ("vcvarsall.bat", "vcbuildtools.bat")
    label = labels.get(version, f"VS{version}")
    vc_dir_candidates: list[Path] = []
    script_candidates: list[Path] = []

    def add_vc_dir_candidate(candidate: Path | str | None):
        candidate = _path_from_value(candidate)
        if candidate is not None:
            vc_dir_candidates.append(candidate)

    def add_script_candidate(candidate: Path | str | None):
        candidate = _path_from_value(candidate)
        if candidate is not None:
            script_candidates.append(candidate)

    def add_path_candidate(candidate: Path | str | None):
        candidate = _path_from_value(candidate)
        if candidate is None:
            return

        if candidate.suffix.lower() == ".bat":
            add_script_candidate(candidate)
            return

        for script_name in script_names:
            add_script_candidate(candidate / script_name)
        add_vc_dir_candidate(candidate)

    def add_vc_dir_from_common_tools(common_tools: str | None):
        if common_tools:
            add_vc_dir_candidate(Path(common_tools) / ".." / ".." / "VC")

    def add_vc_dir_from_install_dir(install_dir: str | None):
        if install_dir:
            add_vc_dir_candidate(Path(install_dir) / ".." / ".." / "VC")

    def add_vc_dir_from_vs_root(vs_root: str | None):
        if vs_root and version in str(Path(vs_root)):
            add_vc_dir_candidate(Path(vs_root) / "VC")

    def add_existing_scripts_under(root: Path):
        if not root.exists():
            return
        try:
            for script_name in script_names:
                for script in root.rglob(script_name):
                    add_script_candidate(script)
        except OSError:
            pass

    add_path_candidate(override)
    for env_var in override_vars.get(version, ()):
        add_path_candidate(os.environ.get(env_var))
    add_vc_dir_from_common_tools(os.environ.get(common_tools_vars.get(version, "")))
    add_vc_dir_from_vs_root(os.environ.get("VSINSTALLDIR"))

    try:
        import winreg

        for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
            for access in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY, 0):
                try:
                    with winreg.OpenKey(hive, r"SOFTWARE\Microsoft\VisualStudio\SxS\VC7", 0,
                                        winreg.KEY_READ | access) as key:
                        value, _ = winreg.QueryValueEx(key, version)
                        add_vc_dir_candidate(Path(value))
                except OSError:
                    pass

                for product in ("VisualStudio", "VCExpress", "WDExpress"):
                    key_root = rf"SOFTWARE\Microsoft\{product}\{version}"
                    for key_path, value_name, add_value in (
                            (key_root, "InstallDir", add_vc_dir_from_install_dir),
                            (key_root + r"\Setup\VC", "ProductDir", add_vc_dir_candidate),
                            (key_root + r"\Setup\VS", "ProductDir", add_vc_dir_from_vs_root),
                    ):
                        try:
                            with winreg.OpenKey(hive, key_path, 0,
                                                winreg.KEY_READ | access) as key:
                                value, _ = winreg.QueryValueEx(key, value_name)
                                add_value(value)
                        except OSError:
                            pass
    except ImportError:
        pass

    program_files_roots = [value for value in (os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles")) if value]
    for program_files_root in program_files_roots:
        root = Path(program_files_root)
        add_vc_dir_candidate(root / f"Microsoft Visual Studio {version}" / "VC")
        add_existing_scripts_under(root / f"Microsoft Visual Studio {version}")
        if version == "12.0":
            add_existing_scripts_under(root / "Microsoft Visual C++ Build Tools 2013")
            add_existing_scripts_under(root / "Microsoft Build Tools" / "12.0")

    for candidate in vc_dir_candidates:
        for script_name in script_names:
            add_script_candidate(candidate / script_name)

    seen: set[Path] = set()
    for candidate in script_candidates:
        resolved_candidate = candidate.resolve()
        if resolved_candidate in seen:
            continue
        seen.add(resolved_candidate)

        if resolved_candidate.exists():
            return resolved_candidate

    msbuild_only_note = ""
    if version == "12.0" and _is_installed_product("Microsoft Build Tools 2013"):
        msbuild_only_note = (
            " Microsoft Build Tools 2013 is installed, but no VS2013 C++ environment batch was found; "
            "that package is not enough for UE 4.5 unless a separate VS2013 C++ toolchain is also installed."
        )

    log_exc(f"Failed to find the {label} C++ toolchain. Expected "
            f"C:\\Program Files (x86)\\Microsoft Visual Studio {version}\\VC\\vcvarsall.bat, "
            f"the matching VisualStudio\\SxS\\VC7\\{version} registry value, or "
            f"{common_tools_vars.get(version, 'VSxxCOMNTOOLS')} pointing at Common7\\Tools. "
            f"If the standalone tools are installed somewhere else, pass {override_args.get(version, 'a vcvarsall override')}."
            f"{msbuild_only_note}")

def _get_vs2015_vcvarsall() -> Path:
    return _get_vs_vcvarsall("14.0")

def _get_vs2013_vcvarsall(override: Path | None = None) -> Path:
    return _get_vs_vcvarsall("12.0", override)

def _capture_vcvarsall_env(vcvarsall: Path, label: str, required_vars: list[str]) -> dict[str, str]:
    _, output = execute(["cmd.exe", "/d", "/c", "call", vcvarsall, "x64", "&&", "set"],
                        success_msg=f"Captured {label} build environment from {vcvarsall}.",
                        fail_msg=f"Failed to capture {label} build environment from {vcvarsall}!",
                        output=ExecuteOutputOptions.SILENT)

    env: dict[str, str] = {}
    for line in output.splitlines():
        key, separator, value = line.partition("=")
        if separator == "" or key.startswith("="):
            continue
        env[key] = value

    env_vars = {key.upper(): value for key, value in env.items()}
    missing = [var for var in required_vars if not env_vars.get(var)]
    if missing:
        log_exc(f"{label} vcvarsall did not provide required environment variables: {', '.join(missing)}")

    return env

def _derive_vc_dir_from_env(vcvarsall: Path, env: dict[str, str]) -> Path | None:
    vc_install_dir = _env_get(env, "VCINSTALLDIR")
    if vc_install_dir:
        return Path(vc_install_dir)

    for parent in vcvarsall.parents:
        if parent.name.lower() == "vc":
            return parent
    return None

def _ensure_vs_common_tools_env(version: str, vcvarsall: Path, env: dict[str, str]) -> None:
    common_tools_vars = {"12.0": "VS120COMNTOOLS", "14.0": "VS140COMNTOOLS"}
    common_tools_var = common_tools_vars[version]
    if _env_get(env, common_tools_var):
        return

    vc_dir = _derive_vc_dir_from_env(vcvarsall, env)
    if vc_dir is None:
        return

    if not _env_get(env, "VCINSTALLDIR"):
        _env_set(env, "VCINSTALLDIR", _path_value(vc_dir))

    if vc_dir.name.lower() == "vc":
        vs_root = vc_dir.parent
        if not _env_get(env, "VSINSTALLDIR"):
            _env_set(env, "VSINSTALLDIR", _path_value(vs_root))
        _env_set(env, common_tools_var, _path_value(vs_root / "Common7" / "Tools"))

def _get_vs2013_env(vs2013_vcvarsall: Path | None = None) -> dict[str, str]:
    global _VS2013_ENV
    if _VS2013_ENV is not None:
        return _VS2013_ENV

    vcvarsall = _get_vs2013_vcvarsall(vs2013_vcvarsall)
    _VS2013_ENV = _capture_vcvarsall_env(
        vcvarsall,
        "VS2013",
        ["INCLUDE", "LIB", "PATH", "VCINSTALLDIR"]
    )
    _ensure_vs_common_tools_env("12.0", vcvarsall, _VS2013_ENV)
    if not _env_get(_VS2013_ENV, "VS120COMNTOOLS"):
        log_exc("VS2013 environment did not provide enough information to synthesize VS120COMNTOOLS.")
    return _VS2013_ENV

def _get_vs2015_env() -> dict[str, str]:
    global _VS2015_ENV
    if _VS2015_ENV is not None:
        return _VS2015_ENV

    _VS2015_ENV = _capture_vcvarsall_env(
        _get_vs2015_vcvarsall(),
        "VS2015",
        ["INCLUDE", "LIB", "PATH", "VS140COMNTOOLS", "VCINSTALLDIR"]
    )
    return _VS2015_ENV

class UPG_54(DefaultUnrealProjectGenerator):
    valid_for = {'5.4', '5.3'}

    @override
    def get_bundled_dotnet(self, platform_dict: dict[str, str] | None = None) -> Path:
        return super().get_bundled_dotnet({'linux': 'linux', 'darwin': 'mac-x64', 'win32': 'windows'} if platform_dict is None else platform_dict)

class UPG_52(UPG_54):
    valid_for = {'5.2', '5.1'}

    @override
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

    @override
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

    @override
    def get_bundled_dotnet(self, platform_dict: dict[str, str] | None = None) -> Path:
        if platform_dict is not None:
            return super().get_bundled_dotnet(platform_dict)
        platform_dict = {'linux': 'Linux', 'darwin': 'Mac', 'win32': 'Windows'}
        return self.git.root / "Engine" / "Binaries" / "ThirdParty" / "DotNet" / platform_dict[self.driver.platform] / "dotnet.exe"

    @override
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

    @override
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
    valid_for = {'4.27', '4.26', '4.25', '4.24'}

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

class UPG_423(UPG_427):
    valid_for = {'4.23', '4.22'}

    @override
    def get_mh_target_cs(self):
        return  """
                    using UnrealBuildTool;
        
                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            if (Target.Platform == UnrealTargetPlatform.Win64)
                            {
                                GlobalDefinitions.Add("__WIN10_GE_DEFINITION__");
                            }
                        }
                    }
                """.replace("__WIN10_GE_DEFINITION__", self._win10_ge_definition) #todo should only do this if win32

    @override
    def get_build_config(self):
        return  """
                    <?xml version="1.0" encoding="utf-8"?>
                    <Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
                      <WindowsPlatform>
                        <Compiler>VisualStudio2019</Compiler>
                        <CompilerVersion>14.29.30133</CompilerVersion>
                      </WindowsPlatform>
                    </Configuration>
                """

class UPG_421(UPG_423):
    valid_for = {'4.21','4.20'}

    @override
    # pyrefly: ignore [bad-override]
    def get_build_config(self):
        return """
                    <?xml version="1.0" encoding="utf-8"?>
                    <Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
                      <WindowsPlatform>
                        <Compiler>VisualStudio2017</Compiler>
                        <CompilerVersion>14.29.30133</CompilerVersion>
                      </WindowsPlatform>
                    </Configuration>
                """

class UPG_419(UPG_421):
    valid_for = {'4.19', '4.18'}

    @override
    # pyrefly: ignore [bad-override]
    def get_build_config(self): # WindowsPlatform doesn't accept Compiler/CompilerVersion from here down
        return None

    @override
    def patch_src(self):
        if self.driver.platform != "win32":
            return
        self.patch_crt_version_selector()
        self.patch_tuple_header()

    def get_vcenv_cs(self):
        return self.git.root / "Engine" / "Source" / "Programs" / "UnrealBuildTool" / "Platform" / "Windows" / "VCEnvironment.cs"

    def patch_crt_version_selector(self):
        target_path = self.get_vcenv_cs()
        if not target_path.exists():
            log_exc(f"Failed to find unreal build tool source directory under {target_path}!")
        text = target_path.read_text(encoding="utf-8")
        old =   """DirectoryInfo LatestIncludeDir = IncludeDirs.OrderBy(x => x.Name).LastOrDefault(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\\\ucrt"));"""
        new =   """Version MaxUniversalCRTVersion = new Version(10, 0, 19041, 0);
                DirectoryInfo LatestIncludeDir = IncludeDirs
                .Where(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\\\ucrt"))
                .Where(n => new Version(n.Name) <= MaxUniversalCRTVersion)
                .OrderBy(n => new Version(n.Name))
                .LastOrDefault();
                """
        if "MaxUniversalCRTVersion" in text:
            return
        if old not in text:
            log_exc(f"Failed to patch UBT Universal CRT selection in {target_path}")
        target_path.write_text(text.replace(old, new), encoding="utf-8")
        logging.info("Patched UBT to cap Universal CRT selection at 10.0.19041.0!")

    def patch_tuple_header(self):
        target_path = self.git.root / "Engine" / "Source" / "Runtime" / "Core" / "Public" / "Templates" / "Tuple.h"
        if not target_path.exists():
            log_exc(f"Failed to find Tuple.h at {target_path}")
        text = target_path.read_text(encoding="utf-8")
        old = """#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)"""
        new = """#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER)"""

        if old not in text:
            log_exc(f"Failed to patch Tuple.h at {target_path}")
        target_path.write_text(text.replace(old, new), encoding="utf-8")
        logging.info("Patched Tuple.h!")

class UPG_417(UPG_419):
    valid_for = {'4.17', '4.16'}

    @override
    def get_vcenv_cs(self):
        return self.git.root / "Engine" / "Source" / "Programs" / "UnrealBuildTool" / "Windows" / "VCEnvironment.cs"

class UPG_415(UPG_417):
    valid_for = {'4.15'}

    @override
    def get_mh_target_cs(self):
        return  """
                    using UnrealBuildTool;
                    using System.Collections.Generic;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target)
                        {
                            Type = TargetType.Game;
                        }

                        public override void SetupBinaries(
                            TargetInfo Target,
                            ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
                            ref List<string> OutExtraModuleNames
                            )
                        {
                            OutExtraModuleNames.Add("MetadataHarness");
                        }
                    }
                """

    @override
    def get_mh_build_cs(self, module_names: str):
        definition = f"Definitions.Add(\"{self._win10_ge_definition}\");" if self.driver.platform == "win32" else ""
        return f"""
            using UnrealBuildTool;
            public class MetadataHarness : ModuleRules
            {{
                public MetadataHarness(TargetInfo Target)
                {{
                    PCHUsage = PCHUsageMode.NoSharedPCHs;
                    MinFilesUsingPrecompiledHeaderOverride = 999999;
                    {definition}

                    PublicDependencyModuleNames.AddRange(new string[]
                    {{
                        {module_names}
                    }});
                }}
            }}
        """

    @override
    def patch_src(self):
        if self.driver.platform != "win32":
            return
        self.patch_crt_version_selector()

class UPG_414(UPG_415):
    valid_for = {'4.14'}

    @override
    def patch_crt_version_selector(self):
        target_path = self.get_vcenv_cs()
        if not target_path.exists():
            log_exc(f"Failed to find unreal build tool source directory under {target_path}!")
        text = target_path.read_text(encoding="utf-8")
        replacements = [
            (
                """DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories().OrderBy(x => x.Name).LastOrDefault();""",
                """Version MaxUniversalCRTVersion = new Version(10, 0, 19041, 0);
                DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories()
                .Where(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\\\ucrt"))
                .Where(n => new Version(n.Name) <= MaxUniversalCRTVersion)
                .OrderBy(n => new Version(n.Name))
                .LastOrDefault();
                """
            ),
            (
                """DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories().OrderBy(x => x.Name).LastOrDefault(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\\\ucrt"));""",
                """Version MaxUniversalCRTVersion = new Version(10, 0, 19041, 0);
                    DirectoryInfo LatestIncludeDir = IncludeDir.EnumerateDirectories()
                    .Where(n => n.Name.All(s => (s >= '0' && s <= '9') || s == '.') && Directory.Exists(n.FullName + "\\\\ucrt"))
                    .Where(n => new Version(n.Name) <= MaxUniversalCRTVersion)
                    .OrderBy(n => new Version(n.Name))
                    .LastOrDefault();
                    """
            )
        ]
        patched = False
        for old, new in replacements:
            if old in text:
                text = text.replace(old, new, 1)
                patched = True

        if text.count("MaxUniversalCRTVersion") < len(replacements):
            log_exc(f"Failed to patch UBT Universal CRT selection in {target_path}")

        if patched:
            target_path.write_text(text, encoding="utf-8")
            logging.info("Patched UBT to cap Universal CRT selection at 10.0.19041.0!")

    @override
    def get_mh_h(self)->str:
        return  """
                #pragma once
                #include "Core.h"
                #include "UObject/ObjectMacros.h"
                """

    @override
    def get_mh_cpp(self, includes: str) -> str:
        return super().get_mh_cpp(includes.replace("CoreMinimal.h", "Core.h"))

class UPG_413(UPG_414):
    valid_for = {'4.13'}

    @override
    def get_env(self):
        env = super().get_env()
        if self.driver.platform == "win32":
            env |= _get_vs2015_env()
        return env

    @override
    def get_mh_h(self)->str:
        return  """
                #pragma once
                #include "CoreUObject.h"
                """

    @override
    def get_mh_cpp(self, includes: str) -> str:
        return super().get_mh_cpp(includes.replace("UObject/Object.h", "CoreUObject.h"))

class UPG_412(UPG_413):
    valid_for = {'4.12', '4.11'}

    @override
    def patch_src(self):
        super().patch_src()
        if self.driver.platform != "win32":
            return
        self.patch_vs2015_comntools_env_fallback()

    def patch_vs2015_comntools_env_fallback(self):
        target_path = self.git.root / "Engine" / "Source" / "Programs" / "UnrealBuildTool" / "Windows" / "UEBuildWindows.cs"
        if not target_path.exists():
            log_exc(f"Failed to find UEBuildWindows.cs at {target_path}")

        text = target_path.read_text(encoding="utf-8")
        if "VS140COMNTOOLS" in text:
            return

        old = """if (VSPath == null)
			{
				return null;
			}

			return new DirectoryInfo(Path.Combine(VSPath, "..", "Tools")).FullName;"""
        new = """if (VSPath == null && VSVersion == 14)
			{
				string EnvPath = Environment.GetEnvironmentVariable("VS140COMNTOOLS");
				if (!String.IsNullOrEmpty(EnvPath))
				{
					return new DirectoryInfo(EnvPath).FullName;
				}
			}

			if (VSPath == null)
			{
				return null;
			}

			return new DirectoryInfo(Path.Combine(VSPath, "..", "Tools")).FullName;"""
        if old not in text:
            log_exc(f"Failed to patch VS2015 common tools lookup in {target_path}")

        target_path.write_text(text.replace(old, new, 1), encoding="utf-8")
        logging.info("Patched UBT to fall back to VS140COMNTOOLS for VS2015 common tools.")

    @override
    def run_generate_clang_database(self):
        if self.driver.platform != "win32":
            super().run_generate_clang_database()
            return

        import xml.etree.ElementTree as ET

        xge_tasks_path = self.git.root / "Engine" / "Intermediate" / "Build" / "XGETasks.xml"
        if not xge_tasks_path.exists():
            log_exc(f"Failed to find XGE task graph for branch {self.branch}: {xge_tasks_path}")

        root = ET.parse(xge_tasks_path).getroot()
        metadata_task = next((task for task in root.findall(".//Task")
                              if task.attrib.get("Caption") == "MetadataAnalysis.cpp"), None)
        if metadata_task is None:
            log_exc(f"Failed to find MetadataAnalysis.cpp task in {xge_tasks_path}")

        tool_name = metadata_task.attrib.get("Tool")
        tool = next((candidate for candidate in root.findall(".//Tool")
                     if candidate.attrib.get("Name") == tool_name), None)
        if tool is None:
            log_exc(f"Failed to find tool {tool_name} for MetadataAnalysis.cpp in {xge_tasks_path}")

        params = tool.attrib.get("Params", "").strip()
        if len(params) == 0:
            log_exc(f"Tool {tool_name} has no command params in {xge_tasks_path}")

        compile_commands = [
            {
                "file": str((self.project_src / "MetadataAnalysis.cpp").resolve()),
                "command": " ".join(["clang-cl.exe", params]),
                "directory": metadata_task.attrib.get("WorkingDir", str((self.git.root / "Engine" / "Source").resolve()))
            }
        ]
        compile_commands_path = self.git.root / "compile_commands.json"
        with open(compile_commands_path, "w", encoding="utf-8") as compile_commands_file:
            json.dump(compile_commands, compile_commands_file, indent=2)
            compile_commands_file.write("\n")

        self.add_parser_additional_commands()
        logging.info(f"Synthesized compile_commands.json for branch {self.branch} from {xge_tasks_path}.")

class UPG_Unsupported(DefaultUnrealProjectGenerator):
    valid_for = {"4.10", "4.9", "4.8", "4.7", "4.6"}

    def __init__(self, branch: str, driver: UnrealDriver):
        super().__init__(branch, driver)
        raise Exception(f"Version {branch} not supported!")

DEP_MAP = {
    "4.5.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.5.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.5.1-release/Required_2of2.zip"
    ),
    "4.5": (
        "https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106747982",
        "https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/266332"
    ),
    "4.4.3": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.3-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.3-release/Required_2of2.zip"
    ),
    "4.4.2": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.2-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.2-release/Required_2of2.zip"
    ),
    "4.4.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.1-release/Required_2of2.zip"
    ),
    "4.4": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.0-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.4.0-release/Required_2of2.zip"
    ),
    "4.3.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.3.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.3.1-release/Required_2of2.zip"
    ),
    "4.3": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.3.0-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.3.0-release/Required_2of2.zip"
    ),
    "4.2.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.2.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.2.1-release/Required_2of2.zip"
    ),
    "4.2": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.2.0-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.2.0-release/Required_2of2.zip"
    ),
    "4.1.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.1.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.1.1-release/Required_2of2.zip"
    ),
    "4.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.1.0-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.1.0-release/Required_2of2.zip"
    ),
    "4.0.2": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.0.2-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.0.2-release/Required_2of2.zip"
    ),
    "4.0.1": (
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.0.1-release/Required_1of2.zip",
        "https://github.com/EpicGames/UnrealEngine/releases/download/4.0.1-release/Required_2of2.zip"
    ),
}
def _github_pat_path() -> Path:
    for candidate in (Path.cwd() / "key.txt", Path(__file__).resolve().with_name("key.txt")):
        if candidate.exists():
            return candidate
    log_exc("Failed to find key.txt containing a GitHub PAT.")

def _github_release_asset_request(url: str) -> urllib.request.Request:
    token = _github_pat_path().read_text(encoding="utf-8").strip()
    if len(token) == 0:
        log_exc("key.txt exists, but it does not contain a GitHub PAT.")
    return urllib.request.Request(
        url,
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/octet-stream",
            "User-Agent": "UEMeta",
        },
    )

def _extract_zip(zip_path: Path, destination: Path, ignore_bad_crc: bool = False):
    with zipfile.ZipFile(zip_path) as zip_:
        for member in zip_.infolist():
            member_name = member.filename.replace("\\", "/")
            try:
                zip_.extract(member, destination)
            except zipfile.BadZipFile as ex:
                if ignore_bad_crc and "Bad CRC-32" in str(ex):
                    logging.warning(f"Ignoring bad CRC while extracting {zip_path.name}; keeping extracted bytes for {member_name}")
                    continue
                raise

def _dependency_zip_cache_path(cache_root: Path, part: int, total_parts: int) -> Path:
    return cache_root / f"Required_{part}of{total_parts}.zip"

def _ensure_release_asset_cached(url: str, zip_path: Path):
    if zip_path.exists():
        if zipfile.is_zipfile(zip_path):
            logging.info(f"Using cached Unreal dependency zip: {zip_path}")
            return
        logging.warning(f"Cached Unreal dependency zip is invalid and will be replaced: {zip_path}")
        zip_path.unlink()

    zip_path.parent.mkdir(parents=True, exist_ok=True)
    download_path = zip_path.with_name(zip_path.name + ".download")
    try:
        with urllib.request.urlopen(_github_release_asset_request(url)) as response:
            with open(download_path, "wb") as download_file:
                shutil.copyfileobj(response, download_file)

        if not zipfile.is_zipfile(download_path):
            log_exc(f"Downloaded Unreal dependency is not a zip file: {url}")

        download_path.replace(zip_path)
        logging.info(f"Cached Unreal dependency zip: {zip_path}")
    except urllib.error.HTTPError as ex:
        log_exc(
            f"Failed to download Unreal dependency zip {url}: GitHub returned HTTP {ex.code}. "
            "Check that key.txt contains a PAT with access to EpicGames/UnrealEngine."
        )
    except urllib.error.URLError as ex:
        log_exc(f"Failed to download Unreal dependency zip {url}: {ex.reason}")
    finally:
        if download_path.exists():
            download_path.unlink(missing_ok=True)

def _download_and_extract_release_asset(url: str, zip_path: Path, destination: Path, ignore_bad_crc: bool = False):
    try:
        _ensure_release_asset_cached(url, zip_path)
        _extract_zip(zip_path, destination, ignore_bad_crc)
    except zipfile.BadZipFile as ex:
        log_exc(f"Failed to extract Unreal dependency zip {zip_path}: {ex}")

class UPG_45(UPG_412): # todo singlefile arg may not be working
    valid_for = {"4.5"}

    @override
    def get_env(self):
        env = DefaultUnrealProjectGenerator.get_env(self)
        if self.driver.platform == "win32":
            env |= _get_vs2013_env(self.driver.vs2013_vcvarsall)
        return env

    @override
    def run_setup(self):
        canonical_branch = _canonical_branch(self.branch)
        deps = DEP_MAP[canonical_branch]
        cache_root = self.driver.intermediate_path / "zips" / canonical_branch
        ignore_bad_crc = True
        logging.info(f"Downloading and unzipping dependencies...")
        for index, url in enumerate(deps, start=1):
            zip_path = _dependency_zip_cache_path(cache_root, index, len(deps))
            _download_and_extract_release_asset(url, zip_path, self.git.root, ignore_bad_crc)

        logging.info(f"Dependencies unzipped!")
