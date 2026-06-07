import git

from Config import GLOBAL_CONFIG
from Git import init_repo
from Util import with_tqdm_logging


def main():
    GLOBAL_CONFIG.initialize()


if __name__ == "__main__":
    with_tqdm_logging(main)