from dataclasses import dataclass
from typing import Literal, Any

from DSO import EnumDeclaration
from Group import NameGroup

@dataclass
class VersionedFieldInstance[T]:
    version: str
    instance: T

@dataclass
class VersionedField[T]:
    versions: list[VersionedFieldInstance[T]]

# assumes all decls are enums
def union_enums(group: NameGroup):
    out: dict[str, str | VersionedField[Any]] = {}
    pass


def union(group: NameGroup) -> object:
    pass