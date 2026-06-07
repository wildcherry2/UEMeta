import argparse
import json
import logging
import os
import subprocess
from enum import StrEnum
from pathlib import Path
from typing import Any

import git
import sys

from Git import GIT

def validate_repo_url(repo_url: str):
    try:
        GIT.ls_remote(repo_url)
        return repo_url
    except git.exc.GitCommandNotFound as e:
        logging.error("`git` command not found, is git installed?")
        raise e
    except git.exc.GitCommandError as e:
        logging.error(f"Failed to find accessible repo at \"{repo_url}\", does the repo exist and is SSH configured if needed?\n"
              f"Error: {e}")
        raise e
    except Exception as e:
        logging.error(f"Failed to validate repo url: {repo_url}!\nError: {e}")
        raise e

def validate_branches(repo_url: str, branches: set[str]):
    try:
        raw_output = GIT.ls_remote("--heads", repo_url)
    except Exception as e:
        logging.error(f"Failed to validate branches because of git error: {e}")
        raise e
    output = set()
    for line in raw_output.splitlines():
        if not line.strip():
            continue

        _sha, ref = line.split(maxsplit=1)
        output.add(ref.rsplit("/", 1)[-1])
    if branches != output:
        raise Exception(f"Some branches are missing: {branches.difference(output)}")

def validate_file_exists(path_str: str):
    if Path(path_str).exists():
        return path_str
    raise argparse.ArgumentTypeError(f"{path_str} does not exist!")

def validate_msvc():
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
    with subprocess.Popen([vswhere, "-utf8", "-json", "-nocolor"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                          bufsize=1) as process:
        if process.stdout:
            for line in process.stdout:
                jsn += line.strip()
    return_code = process.wait()
    if return_code == 0:
        logging.info("vswhere.exe succeeded.")
    else:
        logging.error("Failed vswhere.exe!")

    if len(jsn) == 0:
        raise Exception("vswhere.exe failed to dump json!")

    try:
        jsn: list[dict[str, Any]] = json.loads(jsn)
    except Exception as e:
        logging.error(f"Failed to parse json from vswhere: {e}")
        raise e

    if len(jsn) == 0:
        raise Exception("vswhere.exe failed to dump json!")

    if not any(install['displayName'] == 'Visual Studio Community 2022' for install in jsn):
        raise Exception("Visual Studio 2022 install not found, which is required for Unreal Engine! To install it, "
                        "download an engine version in the epic launcher, "
                        "create a new project, set it to C++, and it'll give you a valid download.")
class Config:
    def __init__(self):
        self.branches: set[str] = set()
        self.repo_url: str = ""
        self.intermediate_path: Path = Path("")
        self.parser_path: Path = Path("")
        self.build_arguments: str = ""
        self.platform = sys.platform
        self.public_dependency_module_names = {"Core", "CoreUObject", "Engine"}
        self.headers: set[str] = set()
        match self.platform:
            case "linux":
                self.platform_shell_ext = "sh"
            case "darwin":
                self.platform_shell_ext = "command"
            case "win32":
                self.platform_shell_ext = "bat"
                validate_msvc()
            case _:
                raise Exception(f"Unsupported platform: {self.platform}")

    def initialize(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--repo-url", type=validate_repo_url, default="git@github.com:EpicGames/UnrealEngine.git",
                            help="The URL to pull branches from. It must exist and you must have permissions to clone "
                                 "and checkout; in other words, if you can't run `git clone {repo_url}`, successfully, "
                                 "this tool won't work.")
        parser.add_argument("--branches", required=True, nargs='+', type=str,
                            help="The branches to generate wrappers for. Must exist and be accessible from the --repo-url.")
        parser.add_argument("--intermediate-directory", type=Path, required=True,
                            help="The directory where intermediate files are stored.")
        parser.add_argument("--parser-path", type=validate_file_exists, required=True,
                            help="The path to the parser executable.")
        parser.add_argument("--build-arguments", nargs='+', type=str,
                            help="Additional build arguments passed to clang-cl when parsing begins. May be needed for "
                                 "non-Windows platforms. These need to be whatever clang-cl needs to work on your platform.")
        parser.add_argument("--public-dependency-module-names", nargs='+', type=str,
                            help="The names of the Unreal modules to include. Defaults are \"Core\", \"CoreUObject\", and "
                                 "\"Engine\". If you override this, the defaults will be erased.")
        parser.add_argument("--headers", nargs='+', type=str, required=True,
                            help="Unreal .h files to include in the analysis.")
        args = parser.parse_args()
        self.repo_url = args.repo_url
        self.branches = set(args.branches)
        self.intermediate_path = args.intermediate_directory.resolve()
        self.parser_path = args.parser_path.resolve()
        self.build_arguments = args.build_arguments
        pdmn = args.public_dependency_module_names
        if pdmn:
            self.public_dependency_module_names = set(pdmn)
        self.headers = set(args.headers)

        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            logging.error(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}")
            raise e

        validate_branches(self.repo_url, self.branches)

GLOBAL_CONFIG = Config()