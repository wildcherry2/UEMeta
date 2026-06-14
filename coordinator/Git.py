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

    def initialize(self, branch_list: list[str]):
        if len(branch_list) == 0:
            raise Exception("Failed to initialize git because the branch list is empty!")
        logging.info(f"Initializing git with branch list: {branch_list}")
        if self.status()[0] == 0:
            logging.info(f"Found repo in root {self.root}...")

            # if there's a repo in the root, validate that the url matches the current remote and checkout the branch
            self.remote = self.__find_remote()
            if self.remote is None:
                branch = branch_list.pop(0)
                logging.info(f"Current repo isn't the one we're processing, cloning repo at {self.url} with branch {branch}...")
                # remote doesn't match up with desired URL, reclone from the URL
                self.clone(branch)
            else:
                # see if the checked out branch matches one in the list
                current_branch = self.current_branch()
                if current_branch in branch_list:
                    logging.info(f"Branch {current_branch} is already checked out. Resetting before initialization completes...")
                    branch_list.remove(current_branch)
                    self.reset()
                    self.checkout(current_branch, force=True)
                    logging.info(f"Checked out {current_branch}, git initialized!")
                else:
                    # current branch is not the one we want, so checkout a branch we want
                    logging.info(f"Current branch ({current_branch}) is not in the branch list ({branch_list}), checking out {branch_list[0]}...")
                    branch = branch_list.pop(0)
                    self.reset()
                    self.checkout(branch)
                    logging.info(f"Checked out {branch}, git initialized!")
        else:
            # there isn't a repo in the root, clone from the URL and checkout the branch
            branch = branch_list.pop(0)
            logging.info(f"No repo found in root, cloning repo at {self.url} with branch {branch}...")
            self.clone(branch)
            logging.info(f"Checked out {branch}, git initialized!")


    # Get the status of the repo root. Can be called before initialization to tell if a repo exists.
    # Will fail if the repo at the root is not a top level repo or if it doesn't exist.
    def status(self, soft_fail=True):
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

    # Checkout the branch. Will look through remote's branch list. Will throw if the repo is not initialized.
    def checkout(self, branch: str, force: bool = False):
        if self.remote is None:
            raise CheckoutException(branch, self.root, "Failed to checkout because the remote is not set!")
        ex = cast(type[ExecuteException], partial(CheckoutException.from_execute, branch, self.root))
        self.__config_long_paths_if_needed()
        if force or self.current_branch() != branch:
            fr = execute(["git", "fetch", "--progress", "--depth", "1", self.remote,
                     f"+refs/heads/{branch}:refs/remotes/{self.remote}/{branch}"], cwd=self.root,
                    output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                    raise_on_error=ex)
            if fr[0] != 0:
                return fr
            return execute(["git", "checkout", "--progress", "-B", branch, f"{self.remote}/{branch}"], cwd=self.root,
                    output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                    raise_on_error=ex)

        return 0, "Branch is already checked out!"

    # Assert that the tuple of branches exists in the remote. Can be used without initialization/instance.
    @staticmethod
    def assert_branches_exist(url: str, branches: Iterable[str]):
        branches_copy = list(dict.fromkeys(branches))
        for match in re.finditer(r"^[^\s]+\s+refs/heads/(?P<branch>.+)$",
                                 execute(["git", "ls-remote", "--heads", url],
                                         output=ExecuteOutputOptions.SILENT)[1], RegexFlag.M):
            branch = match.groupdict()["branch"]
            if branch in branches_copy:
                branches_copy.remove(branch)
                if len(branches_copy) == 0:
                    break

        if not len(branches_copy) == 0:
            raise Exception(f"Missing branches in remote {url}: {branches_copy}")

    # Get the remotes at a url. Can be used without initialization/instance.
    @staticmethod
    def ls_remote(url: str):
        return execute(["git", "ls-remote", url], output=ExecuteOutputOptions.SILENT)

    # Asserts that the url leads to a git repo. Can be used without initialization/instance.
    # Returns a string; this is for argparse.
    @staticmethod
    def assert_remote_exists_with_string_return(url: str):
        # ls_remote will throw if it doesn't exist
        Git.ls_remote(url)
        return url

    # Hard resets the repo and removes untracked/ignored files. Will throw if the repo is not initialized.
    def reset(self):
        self.__config_long_paths_if_needed()
        execute(["git", "clean", "-ffdx"], cwd=self.root, output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)
        return execute(["git", "reset", "--hard"], cwd=self.root, output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)

    # Helper to find the remote name from the url
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

    # Helper to clear and clone a repo.
    def clone(self, branch: str):
        shutil.rmtree(self.root)
        self.root.parent.mkdir(parents=True, exist_ok=True)
        self.remote = "origin"
        ex = cast(type[ExecuteException], partial(CloneException.from_execute, branch, self.url, self.root))
        out = execute(["git", "clone", "--progress", "-c", "core.longpaths=true", "--branch", branch, "--depth", "1",
                 self.url, self.root], cwd=self.root.parent,
                output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
                raise_on_error=ex)
        self.__long_paths_configured = True
        return out

    def __config_long_paths_if_needed(self):
        if not self.__long_paths_configured:
            execute(["git", "config", "core.longPaths", "true"], cwd=self.root)
            self.__long_paths_configured = True

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
