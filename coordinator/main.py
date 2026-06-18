import argparse
import sys

from unreal.UnrealParser import UnrealParser

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", type=str, choices=["unreal"], default="unreal")  # todo use choices more
    args, remaining_args = parser.parse_known_args()
    sys.argv = [sys.argv[0], *remaining_args]
    match args.mode:
        case "unreal":
            driver = UnrealParser()
            driver.start()
        case _:
            raise Exception("Unknown mode!")

if __name__ == "__main__":
    main()
    sys.exit(0)