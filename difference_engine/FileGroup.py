from pathlib import Path
import os

# Flat map of a file directory with only .json files.
class FileGroup:
    def __init__(self, in_directory: Path):
        self.directory = in_directory
        _files: list[Path] = []
        def get_jsons(directory: Path, arr: list[Path]):
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.endswith('.json'):
                        arr.append(Path(os.path.join(root, file)))
                for directory in dirs:
                    get_jsons(Path(os.path.join(root, directory)), arr)
        get_jsons(self.directory, _files)
        self.files: tuple[Path, ...] = tuple(_files)

    def get_files(self) -> tuple[Path, ...]:
        return self.files