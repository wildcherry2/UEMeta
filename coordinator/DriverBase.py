import argparse
import sys
from abc import abstractmethod, ABC
from argparse import Namespace
from pathlib import Path
from typing import Final, Literal, cast, final

import git

from Git import GIT, init_repo, next_branch
from Util import log_exc, exec_proc


def _validate_repo_url(repo_url: str):
    try:
        GIT.ls_remote(repo_url)
        return repo_url
    except git.exc.GitCommandNotFound as e:
        log_exc("`git` command not found, is git installed?")
    except git.exc.GitCommandError as e:
        log_exc(f"Failed to find accessible repo at \"{repo_url}\", does the repo exist and is SSH configured if needed?\n"
              f"Error: {e}", argparse.ArgumentTypeError)
    except Exception as e:
        log_exc(f"Failed to validate repo url: {repo_url}!\nError: {e}", argparse.ArgumentTypeError)

def _validate_file_exists(path_str: str):
    if Path(path_str).exists():
        return path_str
    log_exc(f"{path_str} does not exist!", argparse.ArgumentTypeError)

def _validate_branches(repo_url: str, branches: frozenset[str]):
    try:
        raw_output: str = GIT.ls_remote("--heads", repo_url)
    except Exception as e:
        log_exc(f"Failed to validate branches because of git error: {e}", argparse.ArgumentTypeError)

    output = set()
    for line in raw_output.splitlines():
        line = line.strip()
        _sha, ref = line.split(maxsplit=1)
        output.add(ref.rsplit("/", 1)[-1])

    if branches.issubset(output):
        log_exc(f"Some branches are missing: {branches.difference(output)}", argparse.ArgumentTypeError)

def _generate_parse_command(parser_path: Path, target_cpp: Path, cc: Path, out: Path, addl_cmds: list[str])-> list[str]:
    return [str(parser_path), f"--file \"{target_cpp}\"", f"--compile-commands \"{cc}\"",
            f"--out \"{out}\"", "--split-strategy decl", *addl_cmds]

class DriverBase(ABC):
    def __init__(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--repo-url", type=_validate_repo_url, required=True,
                            help="The URL to pull branches from. It must exist and you must have permissions to clone "
                                 "and checkout; in other words, if you can't run `git clone {repo_url}`, successfully, "
                                 "this tool won't work.")
        parser.add_argument("--branches", required=True, nargs='+', type=str,
                            help="The branches to generate wrappers for. Must exist and be accessible from the --repo-url.")
        parser.add_argument("--intermediate-directory", type=Path, required=True,
                            help="The directory where intermediate files are stored.")
        parser.add_argument("--parser-path", type=_validate_file_exists, required=True,
                            help="The path to the parser executable.")
        parser.add_argument("--parser-additional-commands", type=str, nargs='+',
                            help="Additional commands to pass to the parser.")
        self.with_argument_parser(parser)

        self.args: Final[Namespace] = parser.parse_args()
        self.branches: Final[frozenset[str]] = frozenset(self.args.branches)
        self.repo_url: Final[str] = self.args.repo_url
        self.intermediate_path: Final[Path] = self.args.intermediate_directory.resolve()
        self.parser_path: Final[Path] = self.args.parser_path.resolve()
        self.parser_out: Final[Path] = self.intermediate_path / "parser"
        self.parser_additional_commands: Final[list[str]] = self.args.parser_additional_commands if self.args.parser_additional_commands is not None else list()
        self.platform: Final[Literal["win32", "darwin", "linux"]] = cast(Literal["win32", "darwin", "linux"], sys.platform)

        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            log_exc(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}", argparse.ArgumentTypeError)

        _validate_branches(self.repo_url, self.branches)
        self.__started = False
        self.__repo_initialized = False

    @abstractmethod
    def make_compile_commands(self, branch: str) -> Path:
        pass

    @abstractmethod
    def get_target_cpp(self) -> Path:
        pass

    def with_argument_parser(self, parser: argparse.ArgumentParser):
        pass

    @final
    def start(self):
        if self.__started:
            log_exc("Failed to start driver: already started!")
        self.__started = True

        for branch in self.branches:
            if not self.__repo_initialized:
                self.on_before_init_repo(branch)
                init_repo(branch)
                self.on_after_init_repo(branch)
                self.__repo_initialized = True
            else:
                self.on_before_next_repo(branch)
                next_branch(branch)
                self.on_after_next_repo(branch)

            cc = self.make_compile_commands(branch)
            target_cpp = self.get_target_cpp()
            parser_working_dir = self.parser_out / branch
            parser_working_dir.mkdir(exist_ok=True, parents=True)
            self.on_before_parse(branch)
            exec_proc(_generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir, self.parser_additional_commands),
                      f"Parsing complete for branch {branch}",
                      f"Parsing failed for branch {branch}")
            self.on_after_parse(branch)


    def on_before_init_repo(self, branch: str):
        pass

    def on_after_init_repo(self, branch: str):
        pass

    def on_before_next_repo(self, branch: str):
        pass

    def on_after_next_repo(self, branch: str):
        pass

    def on_before_parse(self, branch: str):
        pass

    def on_after_parse(self, branch: str):
        pass

    def on_before_union(self):
        pass

    def on_after_union(self):
        pass

    def on_before_generation(self):
        pass

    def on_after_generation(self):
        pass