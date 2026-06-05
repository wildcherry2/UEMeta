from dataclasses import dataclass
from itertools import islice
from typing import Literal, Any, cast

from DSO import EnumDeclaration, EnumDeclaration_VF, Enumerator_VF, Enumerator
from Group import NameGroup, File


@dataclass
class VersionedFieldInstance[T = Any]:
    versions: set[str]
    instance: T

@dataclass
class VersionedField[T = Any]:
    instances: list[VersionedFieldInstance[T]]

type UnionResult = dict[str, Any | VersionedField[Any]]

def make_versioned(out: UnionResult, vfs: frozenset[str], version: str):
    for key in out.keys():
        if key in vfs:
            val = out[key]
            out[key] = VersionedField(instances=[VersionedFieldInstance(versions=set(version), instance=val)])

def collapse_versioned(out: UnionResult, vfs: frozenset[str]):
    for key in out.keys():
        if key in vfs and len(out[key].instances) == 1:
            out[key] = out[key].instances[0]

def merge(out: UnionResult, vfs: frozenset[str], json_dict: dict[str, Any], json_version: str):
    for vf in vfs:
        out_field: VersionedField = out[vf]
        current_field = json_dict[vf]
        found = next((v for v in out_field.instances if v.instance == current_field), None)
        if found is None:
            out_field.instances.append(VersionedFieldInstance(instance=current_field, versions=set(json_version)))
        else:
            found.versions.add(json_version)

# assumes all decls are EnumDeclarations and refer to the same enum across versions, and that there are at least
# 2 files in group.values()
def union_enums(files: list[File]):
    enum_vfs = EnumDeclaration_VF
    enumerators_vfs = Enumerator_VF
    out: UnionResult = files[0].get_json()

    make_versioned(out, enum_vfs, files[0].version)
    make_versioned(cast(UnionResult, out['enumerators']), enumerators_vfs, files[0].version)

    # union files + 1 with out object
    for file in islice(files, 1, None):
        json_dict = file.get_json()
        merge(out, enum_vfs, json_dict, file.version)
        merge(cast(UnionResult, out['enumerators']), enumerators_vfs, json_dict['enumerators'], file.version)

    collapse_versioned(out, enum_vfs)
    collapse_versioned(cast(UnionResult, out['enumerators']), enumerators_vfs)

    return out


def union(files: list[File]):
    num_entries = len(files)
    if num_entries == 0:
        return {}
    elif num_entries == 1:
        return files[0].get_json()
    return None