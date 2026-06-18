import argparse
import logging
import sys
from abc import abstractmethod, ABC
from argparse import Namespace
from os import PathLike
from pathlib import Path
from typing import Final, Literal, cast

from Git import Git
from Util import log_exc, execute, ExecuteOutputOptions


class AbstractParser(ABC):
    def __init__(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--repo-url", type=Git.assert_remote_exists_with_string_return, required=True,
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
        logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                            handlers=[logging.StreamHandler(),
                                      logging.FileHandler(Path().cwd() / "coordinator.log", mode='w')],
                            force=True)
        self.branches: list[str] = list(dict.fromkeys(self.args.branches))
        self.repo_url: Final[str] = self.args.repo_url
        self.intermediate_path: Final[Path] = self.args.intermediate_directory.resolve()
        self.parser_path: Final[Path] = self.args.parser_path.resolve()
        self.target_repo_path: Final[Path] = self.intermediate_path / "UnrealEngine"
        self.test_project_path: Final[Path] = self.intermediate_path / "MetadataHarness"
        self.parser_out: Final[Path] = self.intermediate_path / "parser_output"
        self.parser_additional_commands: Final[list[str]] = self.args.parser_additional_commands if self.args.parser_additional_commands is not None else list()
        self.platform: Final[Literal["win32", "darwin", "linux"]] = cast(Literal["win32", "darwin", "linux"], sys.platform)
        self.git = Git(self.intermediate_path, self.repo_url)
        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            log_exc(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}", argparse.ArgumentTypeError)

        self.__started = False

    @abstractmethod
    def make_compile_commands(self, branch: str) -> Path:
        pass

    @abstractmethod
    def get_target_cpp(self) -> Path:
        pass

    def with_argument_parser(self, parser: argparse.ArgumentParser):
        pass

    def checkout(self, branch: str, prevent_checkout_hooks = False):
        self.git.checkout(branch, prevent_hooks=prevent_checkout_hooks, force=True)

    def parse(self, branch: str):
        cc = self.make_compile_commands(branch)
        target_cpp = self.get_target_cpp()
        parser_working_dir = self.parser_out / branch
        parser_working_dir.mkdir(exist_ok=True, parents=True)
        execute(_generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir,
                                        self.parser_additional_commands),
                        success_msg=f"Parsing complete for branch {branch}",
                        fail_msg=f"Parsing failed for branch {branch}", cwd=self.parser_path.parent,
                        output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)

    def start(self):
        if self.__started:
            log_exc("Failed to start driver: already started!")
        self.__started = True
        logging.info("Starting driver!")
        for branch in self.branches:
            self.checkout(branch)
            self.parse(branch)

def _validate_file_exists(path_str: str):
    as_path = Path(path_str)
    if as_path.exists():
        return as_path
    log_exc(f"{path_str} does not exist!", argparse.ArgumentTypeError)

def _generate_parse_command(parser_path: Path, target_cpp: Path, cc: Path, out: Path, addl_cmds: list[str])-> list[str | PathLike]:
    return [parser_path, "--file", target_cpp, "--compile-commands", cc,
            "--out", out, "--split-strategy", "decl", *addl_cmds]

