import sys
import logging
import shutil
import re
from functools import partial
from os import PathLike
from pathlib import Path
from re import RegexFlag
from typing import final, Iterable, cast

from Util import ExecuteOutputOptions, execute, ExecuteException

_REMOTE_LINE = re.compile(r"^(?P<remote_name>\S+)\s+(?P<remote_url>.+?)\s+\((?P<supported_action>fetch|push)\)$")

# Helper to parse a repo name from a url.
def _repo_name_from_url(url: str) -> str:
    name = url.rstrip("/\\").replace("\\", "/").rsplit("/", 1)[-1].rsplit(":", 1)[-1]
    if name.endswith(".git"):
        name = name[:-4]
    if not name:
        raise Exception(f"Could not parse directory name from url: {url}")
    return name

class CloneException(ExecuteException):
    def __init__(self, branch: str, url: str, root: Path, msg: str, return_code: int = 1,
                 expected_return_code: int = 0, stdout: str = ""):
        super().__init__(return_code, expected_return_code, stdout or msg, msg)
        self.branch = branch
        self.url = url
        self.root = root

    @classmethod
    def from_execute(cls, branch: str, url: str, root: Path, return_code: int, expected_return_code: int,
                     stdout: str, msg: str | None = None):
        return cls(branch, url, root, msg if msg is not None else stdout, return_code, expected_return_code, stdout)

class CheckoutException(ExecuteException):
    def __init__(self, branch: str, root: Path, msg: str, return_code: int = 1,
                 expected_return_code: int = 0, stdout: str = ""):
        super().__init__(return_code, expected_return_code, stdout or msg, msg)
        self.branch = branch
        self.root = root

    @classmethod
    def from_execute(cls, branch: str, root: Path, return_code: int, expected_return_code: int,
                     stdout: str, msg: str | None = None):
        return cls(branch, root, msg if msg is not None else stdout, return_code, expected_return_code, stdout)

@final
class Git:
    def __init__(self, init_in: PathLike, url: str):
        if shutil.which("git") is None:
            raise Exception("'git' command not found!")

        self.root = Path(init_in).resolve() / _repo_name_from_url(url)
        self.root.mkdir(parents=True, exist_ok=True)
        self.url = url
        self.remote: str | None = None
        self.__long_paths_configured = False
        self.__remote_branches: list[str] | None = None

    # Get the status of the repo root. Can be called before initialization to tell if a repo exists.
    # Will fail if the repo at the root is not a top level repo or if it doesn't exist when soft_fail is False.
    def status(self, soft_fail=True) -> tuple[int, str]:
        ex = None if soft_fail is True else ExecuteException
        parse_res = execute(["git", "rev-parse", "--show-toplevel"], cwd=self.root, raise_on_error=ex,
                            output=ExecuteOutputOptions.SILENT)
        if parse_res[0] == 0 and Path(parse_res[1].strip()).resolve() == self.root:
            return execute(["git", "status", "--porcelain"], raise_on_error=ex, cwd=self.root,
                           output=ExecuteOutputOptions.SILENT)

        msg = f"Failed to get status for repo root {self.root}"
        if not soft_fail:
            raise Exception(msg)
        return 1, msg

    # Get the name of the current branch. Will throw if the repo is not initialized.
    def current_branch(self):
        return execute(["git", "branch", "--show-current", "--no-color"], cwd=self.root,
                       output=ExecuteOutputOptions.SILENT)[1].strip()

    # Hard resets the repo and removes untracked/ignored files. Will throw if the repo is not initialized.
    def reset(self):
        self.__config_long_paths_if_needed()
        execute(["git", "clean", "-ffdx"], cwd=self.root, output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)
        return execute(["git", "reset", "--hard"], cwd=self.root, output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)

    # Checkout or clone the branch from the url Git was constructed with.
    def checkout(self, branch: str, *, force: bool = False, prevent_hooks: bool = False):
        self.__assert_branch_exists(branch)

        # if we haven't initialized...
        if self.remote is None:
            logging.info(f"Initializing repo at {self.url} for branch {branch}...")
            # if there's a git repo in the root directory...
            if self.status()[0] == 0:
                logging.info(f"Found existing git repo at {self.root}...")
                # check if the repo in the root directory has a remote url that matches our url, and assign its name if true
                self.remote = self.__find_remote()

                # if the checked out repo doesn't belong to the url we have, clone from the url we want (which also clears the root directory first)
                if self.remote is None:
                    logging.info(f"Repo is not the same as remote at {self.url}, clearing and cloning...")
                    self.__clone(branch, prevent_hooks)

                # if the checked out repo belongs to the url we have, ensure that the branch is checked out
                else:
                    logging.info(f"Repo is the same as remote at {self.url}, checking out {branch}...")
                    self.__checkout(branch, True, prevent_hooks)

            # there isn't a git repo in the root directory, so clone
            else:
                logging.info(f"Cloning {self.url} to {self.root}...")
                self.__clone(branch, prevent_hooks)

        # we're initialized, so just checkout
        else:
            logging.info(f"Checking out {branch}...")
            self.__checkout(branch, force, prevent_hooks)

    # Helper to configure git to use long paths. Repo must be initialized.
    def __config_long_paths_if_needed(self):
        if not self.__long_paths_configured:
            execute(["git", "config", "core.longPaths", "true"], cwd=self.root)
            self.__long_paths_configured = True
            logging.info("Long paths configured!")

    # Helper to find the remote name from the url field within a known repo and validate that it's the same as self.url
    def __find_remote(self) -> str | None:
        remotes = execute(["git", "remote", "-v"], cwd=self.root, output=ExecuteOutputOptions.SILENT)[1]
        for line in remotes.splitlines():
            match = _REMOTE_LINE.match(line)
            if match is None:
                continue
            group = match.groupdict()
            if group['remote_url'] == self.url and group['supported_action'] == 'fetch':
                return group['remote_name']
        return None

    # Get the remotes at a url. Can be used without initialization/instance.
    @staticmethod
    def __ls_remote(url: str):
        return execute(["git", "ls-remote", url], output=ExecuteOutputOptions.SILENT)

    # Asserts that the url leads to a git repo. Can be used without initialization/instance.
    # Returns a string; this is for argparse.
    @staticmethod
    def assert_remote_exists_with_string_return(url: str):
        # ls_remote will throw if it doesn't exist
        Git.__ls_remote(url)
        return url

    def __assert_branch_exists(self, branch: str):
        if self.__remote_branches is None:
            self.__remote_branches: list[str] = [match.group('branch') for match in re.finditer(r"^[^\s]+\s+refs/heads/(?P<branch>.+)$",
                                 execute(["git", "ls-remote", "--heads", self.url], output=ExecuteOutputOptions.SILENT)[1], RegexFlag.M)]
            self.__remote_branches += [match.group('tag') for match in re.finditer(r"^[^\s]+\s+refs/tags/(?P<tag>.+)$",
                                       execute(["git", "ls-remote", "--tags", self.url], output=ExecuteOutputOptions.SILENT)[1], RegexFlag.M)]

        if branch in self.__remote_branches:
            return

        raise Exception(f"Branch {branch} does not exist at the remote url {self.url}!")

    def __platform_null_path(self):
        if sys.platform == "win32":
            return "NUL"
        return "/dev/null"

    def __clone(self, branch: str, prevent_hooks: bool):
        shutil.rmtree(self.root)
        self.root.parent.mkdir(parents=True, exist_ok=True)
        self.remote = "origin"
        ex = cast(type[ExecuteException], partial(CloneException.from_execute, branch, self.url, self.root))

        argv: list[str] = ["git"]
        if prevent_hooks:
            argv += ["-c", f"core.hooksPath={self.__platform_null_path()}"]

        argv += ["clone", "--progress", "-c", "core.longpaths=true", "--branch", branch, "--depth", "1",
                       self.url, str(self.root)]

        out = execute(argv, cwd=self.root.parent,
                      output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                      raise_on_error=ex)
        self.__long_paths_configured = True
        return out

    def __checkout(self, branch: str, force: bool, prevent_hooks: bool):
        if self.remote is None:
            raise CheckoutException(branch, self.root, "Failed to checkout because the remote is not set!")
        self.reset()
        ex = cast(type[ExecuteException], partial(CheckoutException.from_execute, branch, self.root))
        self.__config_long_paths_if_needed()
        if force or self.current_branch() != branch:
            fr = execute(["git", "fetch", "--progress", "--depth", "1", self.remote,
                          f"+refs/heads/{branch}:refs/remotes/{self.remote}/{branch}"], cwd=self.root,
                         output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                         raise_on_error=ex)
            if fr[0] != 0:
                return fr
            argv: list[str] = ["git"]
            if prevent_hooks:
                argv += ["-c", f"core.hooksPath={self.__platform_null_path()}"]
            argv += ["checkout", "--progress", "-B", branch, f"{self.remote}/{branch}"]

            return execute(argv, cwd=self.root,
                           output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                           raise_on_error=ex)

        return 0, "Branch is already checked out!"