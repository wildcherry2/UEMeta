import argparse
import sys

import UnrealDriver
from Util import with_tqdm_logging


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", type=str, choices=["unreal"], default="unreal")  # todo use choices more
    args, remaining_args = parser.parse_known_args()
    sys.argv = [sys.argv[0], *remaining_args]
    match args.mode:
        case "unreal":
            driver = UnrealDriver.UnrealDriver()
            driver.start()
        case _:
            raise Exception("Unknown mode!")

if __name__ == "__main__":
    with_tqdm_logging(main)
