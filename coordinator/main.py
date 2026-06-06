import git

from Config import GLOBAL_CONFIG
from Logging import with_tqdm_logging
from Pipeline import init_repo


def main():
    GLOBAL_CONFIG.initialize()
    init_repo()

if __name__ == "__main__":
    with_tqdm_logging(main)