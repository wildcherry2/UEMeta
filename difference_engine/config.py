import argparse
from pathlib import Path
import os

def validate_json_dir(json_dir: str) -> str:
    try:
        as_path = Path(json_dir)
        if not as_path.is_dir():
            raise argparse.ArgumentTypeError(json_dir, f"{json_dir} is not a directory!")
        found_a_json = False
        with os.scandir(as_path) as it:
            for entry in it:
                if entry.is_file() and entry.name.endswith('.json'):
                    found_a_json = True
                    break
        if not found_a_json:
            raise argparse.ArgumentTypeError(json_dir, f"{json_dir} does not contain a JSON file!")
    except Exception as e:
        raise argparse.ArgumentTypeError(json_dir,f"Attempting to validate {json_dir} raised exception: {e}")
    return json_dir

class Config:
    def __init__(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('--json-dirs', type=validate_json_dir, nargs='+', help='Directories containing version-specific json files. Each directory should correspond to a single version.')
        self.__json_dirs = parser.parse_args().json_dirs

    def get_json_dirs(self):
        return self.__json_dirs


GLOBAL_CONFIG = Config()