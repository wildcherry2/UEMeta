from typing import cast

from google.protobuf.message import Message

from proto.VersionedPrimitives_pb2 import (VersionedBool, VersionedUint64List, VersionedStringList,
                                           VersionedString, VersionedUint64, VersionedUint32, VersionedInt64)
from pathlib import Path
from proto.TopLevel_pb2 import (TLFreeFunctionDeclaration, TLRecordDeclaration, TLEnumDeclaration,
                                TLGlobalVariableDeclaration, ForwardDeclarationList, VersionedHashList,
                                VersionedTypeRef, VersionedTypeRefOrAnon)
from proto.Enums_pb2 import (VersionedAccessSpecifier, VersionedConstantEvaluationKind,
                             VersionedEnumScope, VersionedFunctionDefinitionKind, VersionedFunctionStorageClass,
                             VersionedFunctionVirtualityKind, VersionedVariableStorageClass)

type InVersionedList = list[VersionedUint64List] | list[VersionedStringList] | list[VersionedHashList]
type OutVersionedList = VersionedUint64List | VersionedStringList | VersionedHashList | None

type InVersioned = (list[VersionedString] | list[VersionedUint64] | list[VersionedUint32] | list[VersionedInt64] | list[VersionedAccessSpecifier]
                     | list[VersionedConstantEvaluationKind] | list[VersionedEnumScope] | list[VersionedFunctionDefinitionKind]
                     | list[VersionedFunctionStorageClass] | list[VersionedFunctionVirtualityKind] | list[VersionedVariableStorageClass])
type OutVersioned = (VersionedString | VersionedUint64 | VersionedUint32 | VersionedInt64 | VersionedAccessSpecifier
                     | VersionedConstantEvaluationKind | VersionedEnumScope | VersionedFunctionDefinitionKind
                     | VersionedFunctionStorageClass | VersionedFunctionVirtualityKind | VersionedVariableStorageClass)

class Merger:
    def __init__(self, output_dir: Path, version_file_list: list[str]):
        self.version_file_list = Merger.__toProto(version_file_list)
        self.output_dir = output_dir

    def merge(self) -> bool:
        try:
            self.__merge_impl(self.version_file_list)
        except Exception:
            return False
        return True

    def __merge_impl(self, messages: list[Message]):
        for field in messages[0].DESCRIPTOR.fields:
            if field is None or field.message_type is None: continue # null field or field is scalar/enum leaf
            type_name = field.message_type.name
            if type_name == "VersionedBool":
                setattr(messages[0], field.name, self.__merge_versioned_bool([getattr(message, field.name) for message in messages]))
            elif type_name == "VersionedTypeRef":
                setattr(messages[0], field.name, self.__merge_versioned_typeref([getattr(message, field.name) for message in messages]))
            elif type_name == "VersionedTypeRefOrAnon":
                setattr(messages[0], field.name, self.__merge_versioned_typeref_or_anon([getattr(message, field.name) for message in messages]))
            elif type_name.startswith("Versioned"):
                if type_name.endswith("List"):
                    setattr(messages[0], field.name, self.__merge_versioned_list([getattr(message, field.name) for message in messages]))
                else:
                    setattr(messages[0], field.name, self.__merge_versioned([getattr(message, field.name) for message in messages]))
            else:
                self.__merge_impl([getattr(message, field.name) for message in messages])

    def __merge_versioned_bool(self, versioned_bools: list[VersionedBool]):
        dest = None
        while dest is None and len(versioned_bools) > 0:
            dest = versioned_bools.pop(0)
        if dest is None: return None
        for versioned_bool in versioned_bools:
            for true_version in versioned_bool.true_versions:
                dest.true_versions.append(true_version)
            for false_version in versioned_bool.false_versions:
                dest.false_versions.append(false_version)
        return dest

    def __merge_versioned_list(self, versioned_list: InVersionedList) -> OutVersionedList:
        dest = None
        while dest is None and len(versioned_list) > 0:
            dest = versioned_list.pop(0)
        if dest is None: return None

        default_dict = {frozenset(version.value) : version.source_versions for version in dest.versions}
        for versioned_list in versioned_list:
            for version in versioned_list.versions:
                as_set = frozenset(version.value) #todo VersionedHashList might not work right
                existing = default_dict.get(as_set)
                if existing is not None:
                    for this_src_version in version.source_versions:
                        existing.append(this_src_version)
                else:
                    dest.versions.append(version)
                    default_dict[as_set] = version.source_versions

        return dest

    def __merge_versioned_typeref(self, versioned_typerefs: list[VersionedTypeRef]) -> VersionedTypeRef | None:
        dest = None
        while dest is None and len(versioned_typerefs) > 0:
            dest = versioned_typerefs.pop(0)
        if dest is None: return None

    def __merge_versioned_typeref_or_anon(self, versioned_typerefs: list[VersionedTypeRefOrAnon]) -> VersionedTypeRefOrAnon | None:
        pass

    def __merge_versioned(self, versions: InVersioned) -> OutVersioned:
        pass

    @staticmethod
    def __toProto(path_strs: list[str]) -> list[Message]:
        path = Path(path_strs[0])
        fn = None
        match path.suffix:
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
                raise Exception("Unknown file type " + path.suffix)

        def toParsed(ctor, path: str):
            out = ctor()
            with open(path, "rb") as f:
                out.ParseFromString(f.read())
            return out

        return [toParsed(fn, path) for path in path_strs]