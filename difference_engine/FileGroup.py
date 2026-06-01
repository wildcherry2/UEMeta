from pathlib import Path

# Flat map of a file directory with only .json files.
class FileGroup:
    def __init__(self, directory: Path):
        self.dir = directory
        self.files: list[Path] = []