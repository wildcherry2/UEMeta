import logging
from functools import partial
from typing import NoReturn, Union
import git
from git import UpdateProgress, Repo
from tqdm.asyncio import tqdm_asyncio
from tqdm.auto import tqdm

GIT = git.Git()
REPO: Repo | None = None

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

def init_repo(branch: str): #todo: prune all gitignores to make cleanup easier?
    global REPO
    # with tqdm(total=100) as pbar:
    #     try:
    #         git_logger = GitProgressLogger(pbar)
    #         REPO = Repo.clone_from(GLOBAL_CONFIG.repo_url, GLOBAL_CONFIG.intermediate_path,
    #                                partial(GitProgressLogger.log_git_progress, git_logger), branch=branch,
    #                                depth=1)
    #     except Exception as e:
    #         REPO = None
    #         logging.error(f"Failed to clone branch {branch} from repo {GLOBAL_CONFIG.repo_url}: {e}")
    #         raise e

def next_branch(branch: str):
    global REPO
    if REPO is None:
        raise Exception("Failed to go to next branch because REPO is not initialized!")
    try:
        with tqdm(total=100) as pbar:
            git_logger = GitProgressLogger(pbar)
            if REPO.remotes is None or len(REPO.remotes) == 0:
                raise Exception("Failed to go to next branch because no remotes exist!")
            REPO.remotes[0].fetch(branch=branch, depth=1, progress=git_logger)
            REPO.git.checkout("-b", branch, f"origin/{branch}")
    except KeyError:
        return False

    return True