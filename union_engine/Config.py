import argparse
import os
from pathlib import Path
from typing import AnyStr

def directory_contains_json(directory: AnyStr | os.PathLike[AnyStr]) -> bool:
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.json'):
                return True
        for directory in dirs:
            if directory_contains_json(os.path.join(root, directory)):
                return True
    return False

def validate_json_dir(json_dir: str) -> str:
    try:
        as_path = Path(json_dir)
        if not as_path.is_dir():
            raise argparse.ArgumentTypeError(json_dir, f"{json_dir} is not a directory!")
        if not directory_contains_json(as_path):
            raise argparse.ArgumentTypeError(json_dir, f"{json_dir} does not contain a JSON file!")
    except Exception as e:
        raise argparse.ArgumentTypeError(json_dir,f"Attempting to validate {json_dir} raised exception: {e}")
    return json_dir

class Config:
    def __init__(self):
        self.__json_dirs = []

    # initialize explicitly in main() so pools don't try to parse args
    def initialize(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('--json-dirs', type=validate_json_dir, nargs='+', required=True,
                            help='Directories containing version-specific json files. Each directory should correspond to a single version.')
        self.__json_dirs: list[str] = parser.parse_args().json_dirs

    def get_json_dirs(self):
        return self.__json_dirs


GLOBAL_CONFIG = Config()