import argparse
import sys
from pathlib import Path

from CoordinatorConfig import load_config
from unreal.UnrealParserWindows import UnrealParserWindows

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=Path("coordinator.ini"),
                        help="Path to the coordinator config file. Defaults to ./coordinator.ini.")
    parser.add_argument("--git-pat", type=str,
                        help="GitHub PAT to use for GitHub git operations and release asset downloads.")
    args = parser.parse_args()

    config = load_config(args.config, args.git_pat)
    match config.mode.casefold():
        case "unreal":
            driver = UnrealParserWindows(config)
            driver.start()
        case _:
            raise Exception(f"Unknown mode: {config.mode}")

if __name__ == "__main__":
    main()
    sys.exit(0)