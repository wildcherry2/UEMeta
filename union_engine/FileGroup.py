from pathlib import Path
import os
import re

# Individual file information with the module (header) path and hash
class File:
    pattern = re.compile(r"(?P<module>.+)-(?P<hash>.+).json")
    def __init__(self, path: Path):
        self.path: Path = path
        match = File.pattern.match(path.name)
        if match:
            match_dict = match.groupdict()
            self.module = match_dict["module"]
            self.hash = match_dict["hash"]
        else:
            raise Exception(f"File {path} does not have the correct naming format!")

# Flat map of a file directory with only .json files whose name is the version it represents.
# Precondition: in_directory is already validated to be a directory with jsons
class FileGroup:
    def __init__(self, in_directory: Path):
        self.directory = in_directory
        _files: list[File] = []
        def get_jsons(directory: Path, arr: list[File]):
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.endswith('.json'):
                        arr.append(File(Path(os.path.join(root, file))))
                for directory in dirs:
                    get_jsons(Path(os.path.join(root, directory)), arr)
        get_jsons(self.directory, _files)
        self.files = frozenset(_files)
        self.version = in_directory.name

    def get_files(self) -> frozenset[File]:
        return self.files