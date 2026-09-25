from pathlib import Path
from typing import Any

from google.protobuf.internal.containers import RepeatedCompositeFieldContainer, RepeatedScalarFieldContainer
from google.protobuf.message import Message

from proto.Enums_pb2 import (VersionedAccessSpecifier, VersionedConstantEvaluationKind,
                             VersionedEnumScope, VersionedFunctionDefinitionKind, VersionedFunctionStorageClass,
                             VersionedFunctionVirtualityKind, VersionedVariableStorageClass)
from proto.TopLevel_pb2 import (TLFreeFunctionDeclaration, TLRecordDeclaration, TLEnumDeclaration,
                                TLGlobalVariableDeclaration, ForwardDeclarationList, VersionedHashList,
                                Hash, VersionedHash, TemplateParameter, Parameter, Field, BaseSpecifier, MemberFunction,
                                Enumerator, )
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
            out_path = output_dir / Path(version_file_list[0]).name
            out_path = out_path.with_suffix(".m" + out_path.suffix[1:])
            output_dir.mkdir(parents=True, exist_ok=True)
            with open(out_path, "wb") as out_file:
                out_file.write(dest.SerializeToString())

        except Exception as e:
            print(f"Error merging {version_file_list}: {e}")
            return False
        return True

    @staticmethod
    def __merge_impl(dest: Message, messages: list[Message]):
        descriptor = getattr(dest, "DESCRIPTOR", None)
        if descriptor is None: return

        for field in descriptor.fields:
            if field is None or field.message_type is None: continue # null field or field is scalar/enum leaf
            type_name = field.message_type.name
            field_name = field.name
            dest_field = Merger.__getattr(dest, field_name)
            if dest_field is None:
                # find first non-null field value and move it to dest
                for message in messages:
                    new_attr_value = Merger.__getattr(message, field_name)
                    if new_attr_value is None: continue
                    dest_field = getattr(dest, field_name)
                    dest_field.CopyFrom(new_attr_value) #todo is copying necessary
                    message.ClearField(field_name)
                    break
                else:
                    continue

            src_list = [attr for message in messages if (attr := Merger.__getattr(message, field_name)) is not None]
            if type_name == "VersionedBool":
                Merger.__merge_versioned_bool(dest_field, src_list)
            elif type_name == "VersionedHash" or (type_name.startswith("Versioned") and type_name.endswith("List")):
                Merger.__merge_versioned_list_or_hash(dest_field, src_list)
            elif type_name != "VersionedTypeRefOrAnon" and type_name != "VersionedTypeRef" and type_name.startswith("Versioned"):
                Merger.__merge_versioned(dest_field, src_list)
            elif field.is_repeated:
                if type_name == "TemplateParameter" or type_name == "Parameter":
                    Merger.__merge_repeated_positional(dest_field, src_list)
                elif type_name == "Field" or type_name == "BaseSpecifier" or type_name == "MemberFunction" or type_name == "Enumerator":
                    Merger.__merge_repeated_keyed(dest_field, src_list)
                else:
                    print(f"Warning: unhandled repeated type {type_name}")
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
                    default_dict[as_set] = dest.versions[-1].source_versions

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
                    default_dict[version_item.value] = dest.versions[-1].source_versions

    @staticmethod
    def __merge_repeated_positional[T: (TemplateParameter, Parameter)](dest: RepeatedCompositeFieldContainer[T], container_list: list[RepeatedCompositeFieldContainer[T]]):
        dest_len = len(dest)

        # remap container_list such that the 0th index of mapped_container_list contains the 0th T from each list in the container list
        mapped_container_list: list[list[T]] = [[] for _ in range(dest_len)]
        for container in container_list:
            if len(container) != dest_len:
                raise ValueError("Container list must have the same length as dest list!")
            for index, item in enumerate(container):
                mapped_container_list[index].append(item)

        for index, item in enumerate(dest):
            Merger.__merge_impl(item, mapped_container_list[index])

    @staticmethod
    def __merge_repeated_keyed[T: (Field, BaseSpecifier, MemberFunction, Enumerator)](dest: RepeatedCompositeFieldContainer[T], container_list: list[RepeatedCompositeFieldContainer[T]]):
        keyed_src_dict: dict[int | str, list[T]] = dict()
        keyed_dest_dict: dict[int | str, T] = dict()
        for container in container_list:
            for src_item in container:
                keyed_src_dict.setdefault(Merger.__get_key(src_item), []).append(src_item)

        for src_item in dest:
            keyed_dest_dict[Merger.__get_key(src_item)] = src_item

        only_in_srcs = keyed_src_dict.keys() - keyed_dest_dict.keys()
        for exclusive_src in only_in_srcs:
            src_list = keyed_src_dict[exclusive_src]
            if len(src_list) != 1:
                Merger.__merge_impl(src_list[0], src_list[1:])
            dest.append(src_list[0])
            # we don't need to update the dest_dict

        for key, value in keyed_dest_dict.items():
            src_list = keyed_src_dict.get(key, None)
            if src_list is not None:
                Merger.__merge_impl(value, src_list)

    @staticmethod
    def __to_hashable(value: RepeatedScalarFieldContainer[int] | RepeatedScalarFieldContainer[str] | RepeatedCompositeFieldContainer[Hash] | Hash):
        if isinstance(value, Hash):
            return value.a << 64 | value.b

        if not value:
            return frozenset()

        if isinstance(value[0], int):
            return frozenset(value)

        elif isinstance(value[0], str): # get better type checking when in two branches
            return frozenset(value)

        return frozenset([(hsh.a << 64 | hsh.b) for hsh in value])

    @staticmethod
    def __getattr(obj: Message, name: str) -> Any:
        field = obj.DESCRIPTOR.fields_by_name.get(name)
        if field is None:
            return None
        # Repeated fields have no presence; singular message fields use HasField.
        if field.is_repeated or obj.HasField(name):
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
            case _:
                raise Exception("Unknown file type " + test_path.suffix)

        def toParsed(ctor, path: str):
            out = ctor()
            with open(path, "rb") as f:
                out.ParseFromString(f.read())
            return out

        return [proto for path in path_strs if (proto := toParsed(fn, path)) is not None]

    @staticmethod
    def __get_key[T : (Field, BaseSpecifier, MemberFunction, Enumerator)](message: T) -> int | str:
        if isinstance(message, Field):
            if message.name:
                return message.name
            return message.local_occurrence_index.versions[0].value
        if isinstance(message, MemberFunction):
            return message.func_id.a << 64 | message.func_id.b
        if isinstance(message, Enumerator):
            return message.name
        if isinstance(message, BaseSpecifier):
            return message.type_ref.type_name.versions[0].value #__get_key calls happen before any merging
        raise Exception(f"Can't get key for message {message}")