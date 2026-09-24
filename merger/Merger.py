from pathlib import Path
from typing import Any

from google.protobuf.internal.containers import RepeatedCompositeFieldContainer, RepeatedScalarFieldContainer
from google.protobuf.message import Message

from proto.Enums_pb2 import (VersionedAccessSpecifier, VersionedConstantEvaluationKind,
                             VersionedEnumScope, VersionedFunctionDefinitionKind, VersionedFunctionStorageClass,
                             VersionedFunctionVirtualityKind, VersionedVariableStorageClass)
from proto.TopLevel_pb2 import (TLFreeFunctionDeclaration, TLRecordDeclaration, TLEnumDeclaration,
                                TLGlobalVariableDeclaration, ForwardDeclarationList, VersionedHashList,
                                Hash, VersionedHash)
from proto.VersionedPrimitives_pb2 import (VersionedBool, VersionedUint64List, VersionedStringList,
                                           VersionedString, VersionedUint64, VersionedUint32, VersionedInt64)

type InVersionedList = list[VersionedUint64List] | list[VersionedStringList] | list[VersionedHashList]
type OutVersionedList = VersionedUint64List | VersionedStringList | VersionedHashList

type InVersioned = (list[VersionedString] | list[VersionedUint64] | list[VersionedUint32] | list[VersionedInt64] | list[VersionedAccessSpecifier]
                     | list[VersionedConstantEvaluationKind] | list[VersionedEnumScope] | list[VersionedFunctionDefinitionKind]
                     | list[VersionedFunctionStorageClass] | list[VersionedFunctionVirtualityKind] | list[VersionedVariableStorageClass])
type OutVersioned = (VersionedString | VersionedUint64 | VersionedUint32 | VersionedInt64 | VersionedAccessSpecifier
                     | VersionedConstantEvaluationKind | VersionedEnumScope | VersionedFunctionDefinitionKind
                     | VersionedFunctionStorageClass | VersionedFunctionVirtualityKind | VersionedVariableStorageClass)

class Merger:
    @staticmethod
    def merge(output_dir: Path, version_file_list: list[str]) -> bool:
        try:
            version_message_list = Merger.__toProto(version_file_list)
            dest = version_message_list.pop()
            if len(version_message_list) > 0:
                Merger.__merge_impl(dest, version_message_list)

            # note: if we add back occurrence indices to file names, we'll need to regex them out before saving
            out_path = output_dir / version_file_list[0]
            out_path.with_suffix("m" + out_path.suffix)
            with open(out_path, "wb") as out_file:
                out_file.write(dest.SerializeToString())

        except Exception as e:
            print(f"Error merging {version_file_list}: {e}")
            return False
        return True

    @staticmethod
    def __merge_impl(dest: Message, messages: list[Message]):
        if dest.DESCRIPTOR is None: return

        for field in dest.DESCRIPTOR.fields:
            if field is None or field.message_type is None: continue # null field or field is scalar/enum leaf
            type_name = field.message_type.name
            field_name = field.name
            if not hasattr(dest, field_name):
                # find first non-null field value and move it to dest
                found = False
                for message in messages:
                    new_attr_value = getattr(message, field_name, None)
                    if new_attr_value is None: continue
                    found = True
                    setattr(dest, field_name, new_attr_value)
                    delattr(message, field_name)
                if not found: continue

            dest_field = getattr(dest, field_name)
            src_list = [attr for message in messages if (attr := getattr(message, field_name, None)) is not None]
            if type_name == "VersionedBool":
                Merger.__merge_versioned_bool(dest_field, src_list)
            elif type_name == "VersionedHash" or (type_name.startswith("Versioned") and type_name.endswith("List")):
                Merger.__merge_versioned_list_or_hash(dest_field, src_list)
            elif type_name != "VersionedTypeRefOrAnon" and type_name != "VersionedTypeRef" and type_name.startswith("Versioned"):
                Merger.__merge_versioned(dest_field, src_list)
            else:
                Merger.__merge_impl(dest_field, src_list)

    @staticmethod
    def __merge_versioned_bool(dest: VersionedBool, versioned_bools: list[VersionedBool]):
        for versioned_bool in versioned_bools:
            for true_version in versioned_bool.true_versions:
                dest.true_versions.append(true_version)
            for false_version in versioned_bool.false_versions:
                dest.false_versions.append(false_version)

    @staticmethod
    def __merge_versioned_list_or_hash(dest: OutVersionedList | VersionedHash, versioned_list_list: InVersionedList | list[VersionedHash]):
        default_dict = {Merger.__to_hashable(version.value) : version.source_versions for version in dest.versions if version is not None}

        for versioned_list in versioned_list_list:
            for version in versioned_list.versions:
                as_set = Merger.__to_hashable(version.value)
                existing = default_dict.get(as_set)
                if existing is not None:
                    for this_src_version in version.source_versions:
                        existing.append(this_src_version)
                else:
                    # pyrefly: ignore [bad-argument-type]
                    dest.versions.append(version)
                    default_dict[as_set] = version.source_versions

    @staticmethod
    def __merge_versioned(dest: OutVersioned, versions: InVersioned):
        default_dict = { version.value : version.source_versions for version in dest.versions if version is not None}

        for version in versions:
            for version_item in version.versions:
                existing = default_dict.get(version_item.value)
                if existing is not None:
                    for this_src_version in version_item.source_versions:
                        existing.append(this_src_version)
                else:
                    # pyrefly: ignore [bad-argument-type]
                    dest.versions.append(version_item)
                    default_dict[version_item.value] = version_item.source_versions

    @staticmethod
    def __to_hashable(value: RepeatedScalarFieldContainer[int] | RepeatedScalarFieldContainer[str] | RepeatedCompositeFieldContainer[Hash] | Hash):
        if isinstance(value, Hash):
            return value.a << 64 | value.b

        if isinstance(value[0], int):
            return frozenset(value)

        elif isinstance(value[0], str): # get better type checking when in two branches
            return frozenset(value)

        return frozenset([(hsh.a << 64 | hsh.b) for hsh in value])

    @staticmethod
    def __getattr[T](obj: T, name: str) -> Any:
        if hasattr(obj, name):
            return getattr(obj, name)
        return None

    @staticmethod
    def __toProto(path_strs: list[str]) -> list[Message]:
        test_path = Path(path_strs[0])
        fn = None
        match test_path.suffix:
            case ".functionbin":
                fn = TLFreeFunctionDeclaration
            case ".recordbin":
                fn = TLRecordDeclaration
            case ".enumbin":
                fn = TLEnumDeclaration
            case ".varbin":
                fn = TLGlobalVariableDeclaration
            case ".declbin":
                fn = ForwardDeclarationList
            case _:
                raise Exception("Unknown file type " + test_path.suffix)

        def toParsed(ctor, path: str):
            out = ctor()
            with open(path, "rb") as f:
                out.ParseFromString(f.read())
            if out is None:
                print("Failed to parse path: " + path)
            return out

        return [proto for path in path_strs if (proto := toParsed(fn, path)) is not None]