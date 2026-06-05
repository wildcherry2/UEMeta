from dataclasses import dataclass
from itertools import islice
from typing import Literal, Any, cast

from DSO import EnumDeclaration, EnumDeclaration_VF, Enumerator_VF, Enumerator
from Group import NameGroup, File


@dataclass
class VersionedFieldInstance[T]:
    versions: set[str]
    instance: T

@dataclass
class VersionedField[T]:
    instances: list[VersionedFieldInstance[T]]

# assumes all decls are EnumDeclarations and refer to the same enum across versions, and that there are at least
# 2 files in group.values()
def union_enums(files: list[File]):
    enum_vfs = EnumDeclaration_VF
    enumerators_vfs = Enumerator_VF
    out: dict[str, Any | VersionedField[Any]] = files[0].get_json()

    # go ahead and make VersionedFields for fields that we're unioning
    for key in out.keys():
        if key in enum_vfs:
            val = enum_vfs[key]
            enum_vfs[key] = VersionedField[Any](instances=[VersionedFieldInstance[Any](versions=set(files[0].version), instance=val)])

    for key in cast(dict[str, Any | VersionedField[Any]], out['enumerators']).keys():
        if key in enumerators_vfs:
            val = cast(dict[str, Any | VersionedField[Any]], out['enumerators'])[enumerators_vfs[key]]
            cast(dict[str, Any | VersionedField[Any]], out['enumerators'])[enumerators_vfs[key]] \
                = VersionedField[Any](instances=[VersionedFieldInstance[Any](versions=set(files[0].version), instance=val)])

    # union files + 1 with out object
    for file in islice(files, 1, None):
        json_dict = file.get_json()
        for evf in enum_vfs:
            out_field: VersionedField[Any] = out[evf]
            current_field = json_dict[evf]
            found = next((v for v in out_field.instances if v.instance == current_field), None)
            if found is None:
                out_field.instances.append(VersionedFieldInstance[Any](versions=set(file.version), instance=current_field))
            else:
                found.versions.add(file.version)
        for evf in enumerators_vfs:
            out_field: VersionedField[Any] = cast(dict[str, Any | VersionedField[Any]], out['enumerators'])[evf]
            current_field = json_dict['enumerators'][evf]
            found = next((v for v in out_field.instances if v.instance == current_field), None)
            if found is None:
                out_field.instances.append(
                    VersionedFieldInstance[Any](versions=set(file.version), instance=current_field))
            else:
                found.versions.add(file.version)

    # post-processing: if vfs have only one field, collapse them back into a normal dict entry
    for key in out.keys():
        if key in enum_vfs and len(cast(VersionedField[Any], out[key]).instances) == 1:
            out[key] = cast(VersionedField[Any], out[key]).instances[0]

    for key in cast(dict[str, Any | VersionedField[Any]], out['enumerators']).keys():
        if key in enumerators_vfs and len(cast(VersionedField[Any], out['enumerators'][key]).instances) == 1:
            out['enumerators'][key] = out['enumerators'][key].instances[0]

    return out


def union(files: list[File]):
    num_entries = len(files)
    if num_entries == 0:
        return {}
    elif num_entries == 1:
        return files[0].get_json()
    return None