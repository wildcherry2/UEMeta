import argparse
import logging
import sys
from abc import abstractmethod, ABC
from argparse import Namespace
from functools import partial
from os import PathLike
from pathlib import Path
from typing import Final, Literal, cast, final, Union

import git
from git import Repo, UpdateProgress
from tqdm.auto import tqdm
from Util import log_exc, execute


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
            if self.repo is None:
                self.on_before_init_repo(branch)
                self.init_target_repo(branch)
                if not self.repo or not self.repo.working_tree_dir:
                    log_exc("Failed to initialize Repository!")
                self.repo_root = Path(cast(str, self.repo.working_tree_dir)).resolve()
                self.on_after_init_repo(branch)
            else:
                self.on_before_next_repo(branch)
                logging.info("Resetting current repo state...")
                self.repo.git.reset("--hard")
                logging.info(f"Checking out branch {branch}...")
                if not self.next_branch(branch):
                    log_exc(f"Failed to checkout branch {branch}!")
                self.on_after_next_repo(branch)

            cc = self.make_compile_commands(branch)
            target_cpp = self.get_target_cpp()
            parser_working_dir = self.parser_out / branch
            parser_working_dir.mkdir(exist_ok=True, parents=True)
            self.on_before_parse(branch)
            execute(_generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir, self.parser_additional_commands),
                    success_msg=f"Parsing complete for branch {branch}",
                    fail_msg=f"Parsing failed for branch {branch}", cwd=self.parser_path.parent)
            self.on_after_parse(branch)

    @final
    def init_target_repo(self, branch: str):
        if self.repo is not None:
            log_exc(f"Tried to initialize an already initialized repo with branch {branch}!")

        try:
            if self.target_repo_path.exists():
                self.repo = Repo(self.target_repo_path)
                if self.repo.remotes.origin.config_reader.get('url') == self.repo_url:
                    logging.info(f"Repo for url {self.repo_url} already present!")
                    self.next_branch(branch)
                    return
                else:
                    self.repo = None
                    self.target_repo_path.rmdir()

            with tqdm(total=100, unit="%") as pbar:
                git_logger = GitProgressLogger(pbar)
                self.repo = Repo.clone_from(self.repo_url, self.target_repo_path,
                                       progress=git_logger, branch=branch,
                                       depth=1,
                                       multi_options=["--config", "core.longpaths=true"],
                                       allow_unsafe_options=True)
                logging.info(f"Cloned branch {branch}, removing .gitignores...")
                _remove_gitignores(self.repo.working_tree_dir)

        except git.exc.GitCommandError as e:
            log_exc(f"Failed to initialize branch {branch} from repo {self.repo_url} (command exception): {e}")
        except Exception as e:
            log_exc(f"Failed to initialize branch {branch} from repo {self.repo_url} (general exception): {e}")


    @final
    def next_branch(self, branch: str) -> bool:
        if self.repo is None:
            raise Exception("Failed to go to next branch because REPO is not initialized!")
        try:
            if self.repo.active_branch.name == branch:
                logging.info(f"Branch {branch} is already checked out!")
                return True
        except Exception:
            logging.info(f"Failed to get branch name from current repo state, checking out branch {branch}...")

        try:
            logging.info(f"Fetching branch {branch}...")
            with tqdm(total=100, unit="%") as pbar:
                git_logger = GitProgressLogger(pbar)
                if self.repo.remotes is None or len(self.repo.remotes) == 0:
                    raise Exception("Failed to go to next branch because no remotes exist!")
                self.repo.remotes[0].fetch(
                    refspec=f"+refs/heads/{branch}:refs/remotes/origin/{branch}",
                    depth=1,
                    progress=git_logger,
                )
                pbar.moveto(100)
            logging.info(f"Checking out branch {branch}...")
            self.repo.git.checkout("-B", branch, f"origin/{branch}")
            _remove_gitignores(self.repo.working_tree_dir)
        except KeyError:
            return False
        except Exception as e:
            logging.error(f"Failed to go to switch to branch {branch}: {e}!")
            raise e

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
    as_path = Path(path_str)
    if as_path.exists():
        return as_path
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

def _remove_gitignores(root: PathLike | None):
    if root is None:
        return
    working_tree = Path(root)
    (working_tree / ".gitignore").unlink(missing_ok=True)
    for path in working_tree.rglob(".gitignore"):
        path.unlink()

class GitProgressLogger(UpdateProgress):
    def __init__(self, pbar: tqdm):
        super().__init__()
        self.progress = 0.0
        self.pbar = pbar
        self.last_messages: list[str] = []

    def log_git_progress(self, op_code: int, count: str | float,
                         max_count: str | float | None, msg: str = ""):
        if max_count and float(max_count) > 0:
            current_progress = min(100.0, max(0.0, (float(count) / float(max_count)) * 100.0))
            if current_progress < self.progress:
                self.progress = 0.0
                self.pbar.reset(total=100)
            self.pbar.update(current_progress - self.progress)
            self.progress = current_progress
        if msg:
            self.last_messages.append(msg)
            self.last_messages = self.last_messages[-10:]
            self.pbar.set_postfix_str(msg)
        if self._cur_line:
            logging.info(self._cur_line)

    def update(self, op_code: int, cur_count: Union[str, float], max_count: Union[str, float, None] = None,
               message: str = "") -> None:
        self.log_git_progress(op_code, cur_count, max_count, message)
