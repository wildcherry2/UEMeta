import argparse
import logging
import sys
from abc import abstractmethod, ABC
from argparse import Namespace
from functools import partial
from os import PathLike
from pathlib import Path
from typing import Final, Literal, cast, final, NoReturn, Union

import git
from git import Repo, UpdateProgress
from tqdm.asyncio import tqdm_asyncio
from tqdm.auto import tqdm
from Util import log_exc, exec_proc


class DriverBase(ABC):
    def __init__(self):
        self.git = git.Git()
        self.repo: Repo | None = None
        parser = argparse.ArgumentParser()
        parser.add_argument("--repo-url", type=partial(_validate_repo_url, self.git), required=True,
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
        self.branches: Final[tuple[str, ...]] = tuple(dict.fromkeys(self.args.branches))
        self.repo_url: Final[str] = self.args.repo_url
        self.intermediate_path: Final[Path] = self.args.intermediate_directory.resolve()
        self.parser_path: Final[Path] = self.args.parser_path.resolve()
        self.target_repo_path: Final[Path] = self.intermediate_path / "UnrealEngine"
        self.test_project_path: Final[Path] = self.intermediate_path / "MetadataHarness"
        self.parser_out: Final[Path] = self.intermediate_path / "parser_output"
        self.parser_additional_commands: Final[list[str]] = self.args.parser_additional_commands if self.args.parser_additional_commands is not None else list()
        self.platform: Final[Literal["win32", "darwin", "linux"]] = cast(Literal["win32", "darwin", "linux"], sys.platform)
        self.repo_root = self.intermediate_path
        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            log_exc(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}", argparse.ArgumentTypeError)

        _validate_branches(self.git, self.repo_url, self.branches)
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
                self.init_target_repo(branch)
                if not self.repo or not self.repo.working_tree_dir:
                    log_exc("Failed to initialize Repository!")
                self.repo_root = Path(cast(str, self.repo.working_tree_dir)).resolve()
                self.on_after_init_repo(branch)
                self.__repo_initialized = True
            else:
                self.on_before_next_repo(branch)
                self.repo.git.reset("--hard")
                if not self.next_branch(branch):
                    log_exc(f"Failed to checkout branch {branch}!")
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

    @final
    def init_target_repo(self, branch: str):
        if self.repo is not None:
            log_exc(f"Tried to initialize an already initialized repo with branch {branch}!")

        with tqdm(total=100) as pbar:
            try:
                git_logger = GitProgressLogger(pbar)
                self.repo = Repo.clone_from(self.repo_url, self.target_repo_path,
                                       partial(GitProgressLogger.log_git_progress, git_logger), branch=branch,
                                       depth=1)
                logging.info(f"Cloned branch {branch}, removing .gitignores...")

                if self.repo.working_tree_dir is not None:
                    working_tree = Path(self.repo.working_tree_dir)
                    (working_tree / ".gitignore").unlink(missing_ok=True)
                    for path in working_tree.rglob(".gitignore"):
                        path.unlink()

            except Exception as e:
                log_exc(f"Failed to clone branch {branch} from repo {self.repo_url}: {e}")


    @final
    def next_branch(self, branch: str) -> bool:
        if self.repo is None:
            raise Exception("Failed to go to next branch because REPO is not initialized!")
        try:
            with tqdm(total=100) as pbar:
                git_logger = GitProgressLogger(pbar)
                if self.repo.remotes is None or len(self.repo.remotes) == 0:
                    raise Exception("Failed to go to next branch because no remotes exist!")
                self.repo.remotes[0].fetch(
                    refspec=f"+refs/heads/{branch}:refs/remotes/origin/{branch}",
                    depth=1,
                    progress=git_logger,
                )
                self.repo.git.checkout("-b", branch, f"origin/{branch}")
        except KeyError:
            return False

        return True

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

def _validate_repo_url(git_instance: git.Git, repo_url: str):
    try:
        git_instance.ls_remote(repo_url)
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

def _validate_branches(git_instance: git.Git, repo_url: str, branches: tuple[str, ...]):
    try:
        raw_output: str = git_instance.ls_remote("--heads", repo_url)
    except Exception as e:
        log_exc(f"Failed to validate branches because of git error: {e}", argparse.ArgumentTypeError)

    output = set()
    for line in raw_output.splitlines():
        line = line.strip()
        _sha, ref = line.split(maxsplit=1)
        output.add(ref.rsplit("/", 1)[-1])

    missing = set(branches).difference(output)
    if missing:
        log_exc(f"Some branches are missing: {missing}", argparse.ArgumentTypeError)

def _generate_parse_command(parser_path: Path, target_cpp: Path, cc: Path, out: Path, addl_cmds: list[str])-> list[str | PathLike]:
    return [parser_path, "--file", target_cpp, "--compile-commands", cc,
            "--out", out, "--split-strategy", "decl", *addl_cmds]

class GitProgressLogger(UpdateProgress):
    def __init__(self, pbar: tqdm_asyncio[NoReturn]):
        super().__init__()
        self.cnt = 0
        self.pbar = pbar

    def log_git_progress(self, op_code: int, count: str | float,
                         max_count: str | float | None, msg: str):
        diff = float(count) - self.cnt
        self.pbar.update((diff / float(max_count)) if max_count and float(max_count) >= 1 else 0)
        self.cnt += diff
        self.pbar.set_postfix_str(msg)

    def update(self, op_code: int, cur_count: Union[str, float], max_count: Union[str, float, None] = None,
               message: str = "") -> None:
        self.log_git_progress(op_code, cur_count, max_count, message)
