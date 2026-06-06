from dataclasses import dataclass
from itertools import islice
from typing import Literal, Any, cast

from DSO import EnumDeclaration, EnumDeclaration_VF, Enumerator_VF, Enumerator, DeclarationCommon_VF
from Group import NameGroup, File

class VersionSet:
    def __init__(self, initial: tuple[str, Any] | None = None):
        self.__map: dict[Any, set[str]] = {}
        if initial is not None:
            self.add_item(*initial)

    def add_item(self, version: str, instance: Any):
        if self.__map.get(instance) is None:
            self.__map[instance] = {version}
        else:
            self.__map[instance].add(version)

    def get_versions(self, instance: Any):
        return self.__map.get(instance, {})

    def get_collapsed_value(self):
        length = len(self.__map)
        if length != 1:
            return None

        return next(iter(self.__map.keys()))

    def __iter__(self):
        return self.__map.__iter__()

type FullUnionDict = dict[str, VersionSet | FullUnionDict]
type UnionDict = dict[str, Any]

def to_full_union_dict(json: dict[str, Any], version: str) -> FullUnionDict:
    out: FullUnionDict = {}
    for k, v in json.items():
        if isinstance(v, dict):
            out[k] = to_full_union_dict(v, version)
        else:
            out[k] = {VersionSet((version, v))}
    return out

def append_version(out: FullUnionDict, inp: dict[str, Any], version: str):
    for k, v in out.items():
        if isinstance(v, dict):
            append_version(v, inp[k], version)
        else:
            v.add_item(version, inp[k])

def collapse(out: FullUnionDict):
    for k, v in out.items():
        if isinstance(v, dict):
            collapse(v)
        else:
            collapsed = out[k].get_collapsed_value()
            if collapsed is not None:
                out[k] = collapsed

def union(files: list[File]) -> UnionDict:
    num_entries = len(files)
    if num_entries == 0:
        return {}
    elif num_entries == 1:
        return files[0].get_json()

    out = to_full_union_dict(files[0].get_json(), files[0].version)

    for file in islice(files, 1, None):
        append_version(out, file.get_json(), file.version)

    collapse(out)

    return out