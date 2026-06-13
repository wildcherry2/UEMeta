import shutil
import re
from os import PathLike
from pathlib import Path
from typing import final

from Util import ExecuteOutputOptions, execute


_REMOTE_LINE = re.compile(r"^(?P<remote_name>\S+)\s+(?P<remote_url>.+?)\s+\((?P<supported_action>fetch|push)\)$")


@final
class Git:
    def __init__(self, init_in: PathLike, url: str, init_with_branch: str):
        if shutil.which("git") is None:
            raise Exception("'git' command not found!")

        self.root = Path(init_in).resolve() / _repo_name_from_url(url)
        self.root.mkdir(parents=True, exist_ok=True)
        self.url = url
        self.remote: str | None = None
        if self.status()[0] == 0:
            # if there's a repo in the root, validate that the url matches the current remote and checkout the branch
            self.remote = self.__find_remote()
            if self.remote is None:
                # remote doesn't match up with desired URL, reclone from the URL
                self.__clone(init_with_branch)
            else:
                # see if the checked out branch matches one in the list
                if self.current_branch() == init_with_branch:
                    return

                # current branch is not the one we want, so checkout the branch we want
                self.checkout(init_with_branch)
        else:
            # there isn't a repo in the root, clone from the URL and checkout the branch
            self.__clone(init_with_branch)

    def status(self, soft_fail=True):
        parse_res = execute(["git", "rev-parse", "--show-toplevel"], cwd=self.root, raise_on_error=not soft_fail,
                       output=ExecuteOutputOptions.SILENT)
        if parse_res[0] == 0 and Path(parse_res[1].strip()).resolve() == self.root:
            return execute(["git", "status", "--porcelain"], raise_on_error=not soft_fail, cwd=self.root,
                           output=ExecuteOutputOptions.SILENT)

        msg = f"Failed to get status for repo root {self.root}"
        if not soft_fail:
            raise Exception(msg)
        return 1, msg

    def current_branch(self):
        return execute(["git", "branch", "--show-current", "--no-color"], cwd=self.root,
                       output=ExecuteOutputOptions.SILENT)[1].strip()

    def checkout(self, branch: str):
        if self.remote is None:
            raise Exception("Failed to checkout because the remote is not set!")
        if self.current_branch() != branch:
            execute(["git", "fetch", "--depth", "1", self.remote,
                     f"+refs/heads/{branch}:refs/remotes/{self.remote}/{branch}"], cwd=self.root,
                    output=(ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT))
            execute(["git", "checkout", "-B", branch, f"{self.remote}/{branch}"], cwd=self.root,
                    output=(ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT))

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

    def __clone(self, branch: str):
        shutil.rmtree(self.root)
        self.root.parent.mkdir(parents=True, exist_ok=True)
        self.remote = "origin"
        execute(["git", "clone", "--branch", branch, "--depth", "1", self.url, self.root], cwd=self.root.parent,
                output=(ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT))


def _repo_name_from_url(url: str) -> str:
    name = url.rstrip("/\\").replace("\\", "/").rsplit("/", 1)[-1].rsplit(":", 1)[-1]
    if name.endswith(".git"):
        name = name[:-4]
    if not name:
        raise Exception(f"Could not parse directory name from url: {url}")
    return name
