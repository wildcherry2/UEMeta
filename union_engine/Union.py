from dataclasses import dataclass
from typing import Literal

from DSO import EnumDeclaration
from Group import NameGroup

@dataclass
class VersionedEntry[T]:
    versions: list[str]
    value: T

@dataclass
class MergedUnscopedEnumDeclaration:
    kind: Literal["enum"]
    enumerators: list[VersionedEntry[EnumDeclaration]]

# assumes left and right refer to different versions of the same structural entity
def union_enum(left_decl: EnumDeclaration, left_vers: str, right_decl: EnumDeclaration, right_vers: str):
    #todo handle one being scoped and the other not being scoped
    pass


def union(left: NameGroup, right: NameGroup) -> object:
    pass