import logging
import shutil
import re
from os import PathLike
from pathlib import Path
from re import RegexFlag
from typing import final, Iterable

from Util import exec_proc, log_exc


@final
class Git:
    def __init__(self, root: PathLike, url: string, init_with_branch: str):
        if shutil.which("git") is None:
            raise Exception("'git' command not found!")
        self.root = Path(root).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        self.url = url
        self.__fetch_configured = False
        if self.status()[0] == 0:
            # if there's a repo in the root, validate that the url matches the current remote and checkout the branch
            remotes = exec_proc(["git", "remote", "-v"], cwd=self.root)[1]
            for match in re.finditer(r"(?P<remote_name>.+) +(?P<remote_url>http.+\.git) +\((?P<supported_action>.+)\)",
                               remotes, RegexFlag.M | RegexFlag.U):
                group = match.groupdict()
                if group['remote_url'] == self.url and group['supported_action'] == 'fetch':
                    self.remote: str = group['remote_name']
                    break
            if self.remote is None:
                # remote doesn't match up with desired URL, reclone from the URL
                self.__clone(init_with_branch)
            else:
                # see if the checked out branch matches one in the list
                if self.current_branch()[1] == init_with_branch:
                    return

                # current branch is not the one we want, so checkout the branch we want
                self.checkout(init_with_branch)
        else:
            # there isn't a repo in the root, clone from the URL and checkout the branch
            self.__clone(init_with_branch)

    def status(self, soft_fail=True):
        return exec_proc(["git", "status", "--porcelain"], soft_fail=soft_fail, cwd=self.root)

    def current_branch(self):
        return exec_proc(["git", "branch", "--show-current", "--no-color"], cwd=self.root)[1]

    def checkout(self, branch: str):
        if not self.__fetch_configured:
            exec_proc(["git", "config", "remote.origin.fetch", f"+refs/heads/*:refs/remotes/{self.remote}/*"])
            self.__fetch_configured = True
        if self.current_branch()[1] != branch:
            exec_proc(["git", "fetch", "origin", branch, "--depth", "1"])
            exec_proc(["git", "checkout", f"{self.remote}/{branch}"], cwd=self.root)

    def __clone(self, branch: str):
        shutil.rmtree(self.root)
        self.root.mkdir(parents=True, exist_ok=True)
        self.remote = "origin"
        exec_proc(["git", "clone", "--branch", branch, "--depth", "1", self.url], cwd=self.root)