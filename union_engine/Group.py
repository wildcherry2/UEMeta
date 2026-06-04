from pathlib import Path
import os
import re
from typing import Literal, cast, Final
from types import MappingProxyType

from DSO import Declaration, ParserFileMetadataJson, ClassDeclaration, StructDeclaration, UnionDeclaration, \
    FreeFunctionDeclaration, AliasDeclaration, EnumDeclaration, ForwardDeclaration, GlobalDeclaration

FILE_PATTERN = re.compile(r"(?P<qualified_name>.+)-(?P<hash>.+).(?P<type>file|class|struct|union|function|alias|enum|forwardDeclaration|variable)")

# Individual file information with the qualified name, path, hash, version, and loaded pydantic object.
class File:
    def __init__(self, path: Path, version: str):
        self.path: Final[Path] = path
        match = FILE_PATTERN.search(path.name)
        if match:
            match_dict = match.groupdict()
            self.qualified_name: Final[str] = match_dict["qualified_name"]
            self.hash: Final[str] = match_dict["hash"]
            self.__json: Declaration | ParserFileMetadataJson | None = None
            self.version: Final[str] = version
            self.type = cast(Final[Literal["file","class","struct","union","function","alias", "enum","forwardDeclaration","variable"]], match_dict["type"])
        else:
            raise Exception(f"File {path} does not have the correct naming format!")

    def load_if_needed(self):
        if self.__json is not None:
            return
        match self.type:
            case "file":
                self.__json = ParserFileMetadataJson.model_validate_json(self.path.read_text("utf-8"))
            case "class":
                self.__json = ClassDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "struct":
                self.__json = StructDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "union":
                self.__json = UnionDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "function":
                self.__json = FreeFunctionDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "alias":
                self.__json = AliasDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "enum":
                self.__json = EnumDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "forwardDeclaration":
                self.__json = ForwardDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case "variable":
                self.__json = GlobalDeclaration.model_validate_json(self.path.read_text("utf-8"))
            case _:
                raise Exception(f"File {self.path} does not have the correct naming format!")

    def get_json(self):
        return self.__json



# Flat map of a file directory with only .json files whose name is the version it represents.
# Precondition: in_directory is already validated to be a directory with jsons
class VersionGroup:
    def __init__(self, in_directory: Path):
        self.directory: Final[Path] = in_directory
        _files: dict[str, File] = {}
        def get_jsons(directory: Path, arr: dict[str, File]):
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.endswith(("file","class","struct","union","function","alias", "enum","forwardDeclaration","variable")):
                        obj = File(Path(os.path.join(root, file)), in_directory.name)
                        arr[obj.qualified_name] = obj
                for directory in dirs:
                    get_jsons(Path(os.path.join(root, directory)), arr)
        get_jsons(self.directory, _files)
        self.files = MappingProxyType(_files)
        self.version: Final[str] = in_directory.name