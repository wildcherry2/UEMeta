import argparse
import logging
import sys
from abc import abstractmethod, ABC
from argparse import Namespace
from functools import partial
from os import PathLike
from pathlib import Path
from typing import Final, Literal, cast, final, Union, Optional

from Git import Git
from Util import log_exc, execute, ExecuteOutputOptions


class DriverBase(ABC):
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

    @final
    def start(self):
        if self.__started:
            log_exc("Failed to start driver: already started!")
        self.__started = True
        self.on_before_init_repo()
        logging.info("Starting driver!")
        self.git.initialize(self.branches)
        self.git.assert_branches_exist(self.branches)
        initial_branch = self.git.current_branch()
        self.on_after_init_repo(self.git.current_branch())

        def do_parse(in_branch: str):
            cc = self.make_compile_commands(in_branch)
            target_cpp = self.get_target_cpp()
            parser_working_dir = self.parser_out / in_branch
            parser_working_dir.mkdir(exist_ok=True, parents=True)
            self.on_before_parse(in_branch)
            execute(_generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir, self.parser_additional_commands),
                    success_msg=f"Parsing complete for branch {in_branch}",
                    fail_msg=f"Parsing failed for branch {in_branch}", cwd=self.parser_path.parent,
                    output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)
            self.on_after_parse(in_branch)

        self.git.reset()
        _remove_gitignores(self.git.root)
        do_parse(initial_branch)

        for branch in self.branches:
            self.on_before_next_checkout(branch)
            self.git.reset()
            self.git.checkout(branch)
            _remove_gitignores(self.git.root)
            self.on_after_next_checkout(branch)
            do_parse(branch)
        # for branch in self.branches:
        #     if self.repo is None:
        #         self.on_before_init_repo(branch)
        #         self.init_target_repo(branch)
        #         if not self.repo or not self.repo.working_tree_dir:
        #             log_exc("Failed to initialize Repository!")
        #         self.repo_root = Path(cast(str, self.repo.working_tree_dir)).resolve()
        #         self.on_after_init_repo(branch)
        #     else:
        #         self.on_before_next_repo(branch)
        #         logging.info("Resetting current repo state...")
        #         self.repo.git.reset("--hard")
        #         logging.info(f"Checking out branch {branch}...")
        #         if not self.next_branch(branch):
        #             log_exc(f"Failed to checkout branch {branch}!")
        #         self.on_after_next_repo(branch)
        #
        #     cc = self.make_compile_commands(branch)
        #     target_cpp = self.get_target_cpp()
        #     parser_working_dir = self.parser_out / branch
        #     parser_working_dir.mkdir(exist_ok=True, parents=True)
        #     self.on_before_parse(branch)
        #     execute(_generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir, self.parser_additional_commands),
        #             success_msg=f"Parsing complete for branch {branch}",
        #             fail_msg=f"Parsing failed for branch {branch}", cwd=self.parser_path.parent)
        #     self.on_after_parse(branch)

    def on_before_init_repo(self):
        pass

    def on_after_init_repo(self, branch: str):
        pass

    def on_before_next_checkout(self, branch: str):
        pass

    def on_after_next_checkout(self, branch: str):
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

def _validate_file_exists(path_str: str):
    as_path = Path(path_str)
    if as_path.exists():
        return as_path
    log_exc(f"{path_str} does not exist!", argparse.ArgumentTypeError)

def _generate_parse_command(parser_path: Path, target_cpp: Path, cc: Path, out: Path, addl_cmds: list[str])-> list[str | PathLike]:
    return [parser_path, "--file", target_cpp, "--compile-commands", cc,
            "--out", out, "--split-strategy", "decl", *addl_cmds]

def _remove_gitignores(root: PathLike | None):
    if root is None:
        return
    working_tree = Path(root)
    (working_tree / ".gitignore").unlink(missing_ok=True)
    for path in working_tree.rglob(".gitignore"):
        path.unlink()