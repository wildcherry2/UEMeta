import argparse
import logging
from enum import StrEnum
from pathlib import Path

import git
from Git import GIT

class Mode(StrEnum):
    UNREAL = "unreal"
    DEFAULT = "default"

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

class Config:
    def __init__(self):
        self.branches: set[str] = set()
        self.repo_url: str = ""
        self.intermediate_path: Path = Path("")
        self.parser_path: Path = Path("")
        self.mode: Mode = Mode.DEFAULT
        self.build_arguments: str = ""

    def initialize(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--repo-url", type=validate_repo_url, required=True,
                            help="The URL to pull branches from. It must exist and you must have permissions to clone "
                                 "and checkout; in other words, if you can't run `git clone {repo_url}`, successfully, "
                                 "this tool won't work.")
        parser.add_argument("--branches", required=True, nargs='+', type=str,
                            help="The branches to generate wrappers for. Must exist and be accessible from the --repo-url.")
        parser.add_argument("--intermediate-directory", type=Path, required=True,
                            help="The directory where intermediate files are stored.")
        parser.add_argument("--parser-path", type=validate_file_exists, required=True,
                            help="The path to the parser executable.")
        parser.add_argument("--mode", type=Mode, default=Mode.DEFAULT,
                            help="The mode to run the tool in. If not given or `default`, complete and valid clang-cl (MSVC) "
                                 "--build-arguments must be given.")
        parser.add_argument("--build-arguments", nargs='+', type=str,
                            help="If --mode is `unreal`, these are additional build arguments passed to clang-cl when "
                                 "parsing begins, and is optional.\nIf --mode is `default` or not given, these are "
                                 "all of the build commands passed to clang-cl when parsing begins, and are required.")
        args = parser.parse_args()
        self.repo_url = args.repo_url
        self.branches = set(args.branches)
        self.intermediate_path = args.intermediate_directory
        self.parser_path = args.parser_path
        self.mode = args.mode
        self.build_arguments = args.build_arguments

        if self.mode == Mode.DEFAULT:
            raise Exception(f"Unhandled mode (for now): {self.mode}")

        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            logging.error(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}")
            raise e

        validate_branches(self.repo_url, self.branches)

GLOBAL_CONFIG = Config()