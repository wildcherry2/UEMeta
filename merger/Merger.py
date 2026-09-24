from pathlib import Path

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

type InVersionedList = list[VersionedUint64List | None] | list[VersionedStringList | None] | list[VersionedHashList | None]
type OutVersionedList = VersionedUint64List | VersionedStringList | VersionedHashList | None

type InVersioned = (list[VersionedString | None] | list[VersionedUint64 | None] | list[VersionedUint32 | None] | list[VersionedInt64 | None] | list[VersionedAccessSpecifier | None]
                     | list[VersionedConstantEvaluationKind | None] | list[VersionedEnumScope | None] | list[VersionedFunctionDefinitionKind | None]
                     | list[VersionedFunctionStorageClass | None] | list[VersionedFunctionVirtualityKind | None] | list[VersionedVariableStorageClass | None])
type OutVersioned = (VersionedString | VersionedUint64 | VersionedUint32 | VersionedInt64 | VersionedAccessSpecifier
                     | VersionedConstantEvaluationKind | VersionedEnumScope | VersionedFunctionDefinitionKind
                     | VersionedFunctionStorageClass | VersionedFunctionVirtualityKind | VersionedVariableStorageClass)

class Merger:
    def __init__(self, output_dir: Path, version_file_list: list[str]):
        self.version_file_list = Merger.__toProto(version_file_list)
        self.output_dir = output_dir

    def merge(self) -> bool:
        try:
            # pyrefly: ignore [bad-argument-type]
            self.__merge_impl(self.version_file_list)
        except Exception:
            return False
        return True

    def __merge_impl(self, messages: list[Message]):
        for field in messages[0].DESCRIPTOR.fields:
            if field is None or field.message_type is None: continue # null field or field is scalar/enum leaf
            type_name = field.message_type.name
            if type_name == "VersionedBool":
                setattr(messages[0], field.name, self.__merge_versioned_bool([self.__getattr(message, field.name) for message in messages]))
            elif type_name == "VersionedHash":
                setattr(messages[0], field.name, self.__merge_versioned_list_or_hash([self.__getattr(message, field.name) for message in messages]))
            elif type_name.startswith("Versioned") and type_name.endswith("List"):
                setattr(messages[0], field.name, self.__merge_versioned_list_or_hash([self.__getattr(message, field.name) for message in messages]))
            elif type_name != "VersionedTypeRefOrAnon" and type_name != "VersionedTypeRef" and type_name.startswith("Versioned"):
                setattr(messages[0], field.name, self.__merge_versioned([self.__getattr(message, field.name) for message in messages]))
            else:
                self.__merge_impl([nonnull_message for message in messages if (nonnull_message := self.__getattr(message, field.name)) is not None])

    def __merge_versioned_bool(self, versioned_bools: list[VersionedBool | None]):
        dest = None
        while dest is None and len(versioned_bools) > 0:
            dest = versioned_bools.pop(0)
        if dest is None: return None

        for versioned_bool in versioned_bools:
            if versioned_bool is None: continue
            for true_version in versioned_bool.true_versions:
                dest.true_versions.append(true_version)
            for false_version in versioned_bool.false_versions:
                dest.false_versions.append(false_version)
        return dest

    def __merge_versioned_list_or_hash(self, versioned_list_list: InVersionedList | list[VersionedHash | None]) -> OutVersionedList | VersionedHash:
        dest = None
        while dest is None and len(versioned_list_list) > 0:
            dest = versioned_list_list.pop(0)
        if dest is None: return None

        default_dict = {self.__to_hashable(version.value) : version.source_versions for version in dest.versions if version is not None}

        for versioned_list in versioned_list_list:
            if versioned_list is None: continue
            for version in versioned_list.versions:
                as_set = self.__to_hashable(version.value)
                existing = default_dict.get(as_set)
                if existing is not None:
                    for this_src_version in version.source_versions:
                        existing.append(this_src_version)
                else:
                    # pyrefly: ignore [bad-argument-type]
                    dest.versions.append(version)
                    default_dict[as_set] = version.source_versions

        return dest

    def __merge_versioned(self, versions: InVersioned) -> OutVersioned | None:
        dest = None
        while dest is None and len(versions) > 0:
            dest = versions.pop(0)
        if dest is None: return None
        default_dict = { version.value : version.source_versions for version in dest.versions if version is not None}
        for version in versions:
            if version is None: continue
            for version_item in version.versions:
                existing = default_dict.get(version_item.value)
                if existing is not None:
                    for this_src_version in version_item.source_versions:
                        existing.append(this_src_version)
                else:
                    # pyrefly: ignore [bad-argument-type]
                    dest.versions.append(version_item)
                    default_dict[version_item.value] = version_item.source_versions

        return dest

    def __to_hashable(self, value: RepeatedScalarFieldContainer[int] | RepeatedScalarFieldContainer[str] | RepeatedCompositeFieldContainer[Hash] | Hash):
        if isinstance(value, Hash):
            return value.a << 64 | value.b

        if isinstance(value[0], int):
            return frozenset(value)

        elif isinstance(value[0], str): # get better type checking when in two branches
            return frozenset(value)

        return frozenset([(hsh.a << 64 | hsh.b) for hsh in value])

    def __getattr[T, V](self, obj: T, name: str) -> V | None:
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