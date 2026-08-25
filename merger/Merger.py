import re
from typing import cast

from google.protobuf.internal.containers import RepeatedCompositeFieldContainer

from proto.TopLevel_pb2 import TLEnumDeclaration, DeclarationMetadata, TLAliasDeclaration, Identifier, TemplateDetails, \
    TLForwardDeclaration, TLFileData, TLFreeFunctionDeclaration, TLGlobalVariableDeclaration, TypeInfo, \
    TLRecordDeclaration, TemplateParameter, FunctionCommon, EnumDetails
from proto.VersionedPrimitives_pb2 import VersionedBool, VersionedUint64List, VersionedStringList
from pathlib import Path

class ProtoWrapper:
    def __init__(self, input_dir: Path, file_name: str):
        meta_match = ProtoWrapper.file_meta_re.match(file_name)
        if meta_match is None:
            raise Exception(f"File {file_name} does not have the correct format")
        self.file_name = file_name
        self.id_hash = meta_match.group("id_hash")
        self.content_hash = meta_match.group("content_hash")
        self.path_hash = meta_match.group("path_hash")
        self.OI = meta_match.group("OI")
        self.type = meta_match.group("type")
        match self.type:
            case "fwdecl":
                self.obj = TLForwardDeclaration()
            case "alias":
                self.obj = TLAliasDeclaration()
            case "class":
                self.obj = TLRecordDeclaration()
            case "enum":
                self.obj = TLEnumDeclaration()
            case "file":
                self.obj = TLFileData()
            case "function":
                self.obj = TLFreeFunctionDeclaration()
            case "struct":
                self.obj = TLRecordDeclaration()
            case "var":
                self.obj = TLGlobalVariableDeclaration()
            case _:
                raise Exception(f"Unknown type: {self.type}")

        with open(input_dir / file_name, "rb") as file_name:
            self.obj.ParseFromString(file_name.read())

    def write_obj(self, output_dir: Path):
        with open((output_dir / self.id_hash).with_suffix(f"{self.type}bin"), "wb") as file_name:
            file_name.write(self.obj.SerializeToString())

    def merge_with(self, other: ProtoWrapper):
        if self.id_hash != other.id_hash or self.type != other.type:
            raise Exception(f"Mismatch in id_hash or type! this hash = {self.id_hash}, other hash = {other.id_hash}, this type = {self.type}, other type = {other.type}")
        match self.type:
            case "alias":
                ProtoWrapper.__merge_alias(cast(TLAliasDeclaration, self.obj), cast(TLAliasDeclaration, other.obj))
            case "class":
                ProtoWrapper.__merge_cls(cast(TLRecordDeclaration, self.obj), cast(TLRecordDeclaration, other.obj))
            case "struct":
                ProtoWrapper.__merge_cls(cast(TLRecordDeclaration, self.obj), cast(TLRecordDeclaration, other.obj))
            case "fwdecl":
                ProtoWrapper.__merge_fwdecl(cast(TLForwardDeclaration, self.obj), cast(TLForwardDeclaration, other.obj))
            case "enum":
                ProtoWrapper.__merge_enum(cast(TLEnumDeclaration, self.obj), cast(TLEnumDeclaration, other.obj))
            case "function":
                ProtoWrapper.__merge_fn(cast(TLFreeFunctionDeclaration, self.obj), cast(TLFreeFunctionDeclaration, other.obj))
            case "var":
                ProtoWrapper.__merge_var(cast(TLGlobalVariableDeclaration, self.obj), cast(TLGlobalVariableDeclaration, other.obj))
            case "file":
                ProtoWrapper.__merge_file(cast(TLFileData, self.obj), cast(TLFileData, other.obj))
            case _:
                raise Exception(f"Unknown type: {self.type}")

    @staticmethod
    def __nullity_matches(into: object | None, src: object | None, label: str):
        if (into is None and src is not None) or (into is not None and src is None):
            raise Exception(f"{label} nullity mismatch between {into} and {src}")

    @staticmethod
    def __merge_alias(into: TLAliasDeclaration, src: TLAliasDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)
        ProtoWrapper.__merge_typeinfo(into.aliased_type, src.aliased_type)
        ProtoWrapper.__merge_versioned(into.alias, src.alias)
        ProtoWrapper.__merge_versioned(into.as_string, src.as_string)

        ProtoWrapper.__nullity_matches(into.template_details, src.template_details, "Alias template details")

        if into.template_details != None:
            ProtoWrapper.__merge_template_params(into.template_details.parameters, src.template_details.parameters)


    @staticmethod
    def __merge_cls(into: TLRecordDeclaration, src: TLRecordDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)
        ProtoWrapper.__merge_versioned(into.is_complete_definition, src.is_complete_definition)
        ProtoWrapper.__merge_versioned(into.size_bytes, src.size_bytes)
        ProtoWrapper.__merge_versioned(into.align_bytes, src.align_bytes)
        ProtoWrapper.__merge_versioned(into.nested_hashes, src.nested_hashes)

        if into.template_details != None:
            ProtoWrapper.__merge_template_params(into.template_details.parameters, src.template_details.parameters)

        # handle fields
        into_fields = { obj.identifier.qualified_name_hash: obj for obj in into.fields }
        src_fields = { obj.identifier.qualified_name_hash: obj for obj in src.fields }
        for field in src_fields.values():
            into_field = into_fields.get(field.identifier.qualified_name_hash, None)
            if into_field is None:
                into.fields.append(field)
                continue
            ProtoWrapper.__merge_ident(into_field.identifier, field.identifier)
            ProtoWrapper.__merge_versioned(into_field.as_string, field.as_string)
            ProtoWrapper.__merge_versioned(into_field.access, field.access)
            ProtoWrapper.__merge_versioned(into_field.is_mutable, field.is_mutable)
            ProtoWrapper.__merge_versioned(into_field.is_bitfield, field.is_bitfield)
            ProtoWrapper.__merge_versioned(into_field.bit_width, field.bit_width)
            ProtoWrapper.__merge_versioned(into_field.offset_bits, field.offset_bits)
            ProtoWrapper.__merge_versioned(into_field.content_hash, field.content_hash)
            ProtoWrapper.__merge_versioned(into_field.default_value, field.default_value)
            ProtoWrapper.__merge_typeinfo(into_field.type_info, field.type_info)

        # handle methods
        into_methods = { obj.common.identifier.qualified_name_hash: obj for obj in into.methods }
        src_fields = { obj.common.identifier.qualified_name_hash: obj for obj in src.methods }
        for method in src_fields.values():
            into_method = into_methods.get(method.common.identifier.qualified_name_hash, None)
            if into_method is None:
                into.methods.append(method)
                continue
            ProtoWrapper.__merge_versioned(into_method.access, method.access)
            ProtoWrapper.__merge_versioned(into_method.is_const, method.is_const)
            ProtoWrapper.__merge_versioned(into_method.is_volatile, method.is_volatile)
            ProtoWrapper.__merge_versioned(into_method.virtuality, method.virtuality)
            ProtoWrapper.__merge_versioned(into_method.is_deleted, method.is_deleted)
            ProtoWrapper.__merge_versioned(into_method.vtable_index.index, method.vtable_index.index)
            ProtoWrapper.__merge_versioned(into_method.vtable_index.offset, method.vtable_index.offset)
            ProtoWrapper.__merge_function_common(into_method.common, method.common)

        # handle base specifier
        into_bases = { obj.identifier.qualified_name_hash: obj for obj in into.bases }
        src_bases = { obj.identifier.qualified_name_hash: obj for obj in src.bases }
        for base in src_bases.values():
            into_base = into_bases.get(base.identifier.qualified_name_hash, None)
            if into_base is None:
                into.bases.append(base)
                continue
            ProtoWrapper.__merge_ident(into_base.identifier, base.identifier)
            ProtoWrapper.__merge_versioned(into_base.is_virtual, base.is_virtual)
            ProtoWrapper.__merge_versioned(into_base.offset, base.offset)
            ProtoWrapper.__merge_versioned(into_base.as_string, base.as_string)
            ProtoWrapper.__merge_typeinfo(into_base.type_info, base.type_info)

    @staticmethod
    def __merge_fwdecl(into: TLForwardDeclaration, src: TLForwardDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)
        ProtoWrapper.__nullity_matches(into.template_details, src.template_details, "Forward declaration template details")
        if into.template_details is not None:
            ProtoWrapper.__merge_template_params(into.template_details.parameters, src.template_details.parameters)

        ProtoWrapper.__merge_versioned(into.as_string, src.as_string)

        ProtoWrapper.__nullity_matches(into.enum_details, src.enum_details, "Forward declaration enum details")
        if into.enum_details is not None:
            ProtoWrapper.__merge_enum_details(into.enum_details, src.enum_details)


    @staticmethod
    def __merge_enum(into: TLEnumDeclaration, src: TLEnumDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)
        ProtoWrapper.__merge_enum_details(into.details, src.details)

        into_enumerators = { obj.identifier.qualified_name_hash: obj for obj in into.enumerators }
        src_enumerators = { obj.identifier.qualified_name_hash: obj for obj in src.enumerators }
        for enumerator in src_enumerators.values():
            into_enumerator = into_enumerators.get(enumerator.identifier.qualified_name_hash, None)
            if into_enumerator is None:
                into.enumerators.append(enumerator)
                continue
            ProtoWrapper.__merge_ident(into_enumerator.identifier, enumerator.identifier)
            ProtoWrapper.__merge_versioned(into_enumerator.value, enumerator.value)

    @staticmethod
    def __merge_fn(into: TLFreeFunctionDeclaration, src: TLFreeFunctionDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)
        ProtoWrapper.__merge_function_common(into.common, src.common)

    @staticmethod
    def __merge_var(into: TLGlobalVariableDeclaration, src: TLGlobalVariableDeclaration):
        ProtoWrapper.__merge_metadata(into.metadata, src.metadata)

        ProtoWrapper.__nullity_matches(into.template_details, src.template_details, "Global variable template details")
        if into.template_details is not None:
            ProtoWrapper.__merge_template_params(into.template_details.parameters, src.template_details.parameters)

        ProtoWrapper.__merge_typeinfo(into.type_info, src.type_info)
        ProtoWrapper.__merge_versioned(into.as_string, src.as_string)
        ProtoWrapper.__merge_versioned(into.storage_class, src.storage_class)
        ProtoWrapper.__merge_versioned(into.default_value, src.default_value)
        ProtoWrapper.__merge_versioned(into.constant_evaluation_kind, src.constant_evaluation_kind)
        ProtoWrapper.__merge_versioned(into.content_hash, src.content_hash)

    @staticmethod
    def __merge_file(into: TLFileData, src: TLFileData):
        ProtoWrapper.__merge_versioned(into.file_occurrence, src.file_occurrence)
        ProtoWrapper.__merge_versioned(into.defined_type_hashes, src.defined_type_hashes)
        ProtoWrapper.__merge_versioned(into.forward_declaration_hashes, src.forward_declaration_hashes)
        ProtoWrapper.__merge_versioned(into.builtin_includes, src.builtin_includes)

    @staticmethod
    def __merge_typeinfo(into: TypeInfo, src: TypeInfo):
        ProtoWrapper.__merge_versioned(into.type, src.type)
        ProtoWrapper.__merge_versioned(into.underlying_type, src.underlying_type)
        ProtoWrapper.__merge_versioned(into.source_path_hash, src.source_path_hash)

    @staticmethod
    def __merge_metadata(into: DeclarationMetadata, src: DeclarationMetadata):
        ProtoWrapper.__merge_versioned(into.content_hash, src.content_hash)
        ProtoWrapper.__merge_versioned(into.occurrence_index, src.occurrence_index)
        ProtoWrapper.__merge_ident(into.identifier, src.identifier)

    @staticmethod
    def __merge_ident(into: Identifier, src: Identifier):
        ProtoWrapper.__merge_versioned(into.documentation, src.documentation)
        ProtoWrapper.__merge_versioned(into.file_path, src.file_path)
        ProtoWrapper.__merge_versioned(into.file_path_hash, src.file_path_hash)

    @staticmethod
    def __merge_enum_details(into: EnumDetails, src: EnumDetails):
        ProtoWrapper.__merge_versioned(into.underlying_type, src.underlying_type)

    @staticmethod
    def __merge_template_params(into: RepeatedCompositeFieldContainer[TemplateParameter], src: RepeatedCompositeFieldContainer[TemplateParameter]):
        if len(into) != len(src):
            raise Exception(f"Failed to merge template details {into} and {src} because number of parameters aren't the same!")
        for i in range(len(into)):
            current_into = into[i]
            current_src = src[i]
            ProtoWrapper.__merge_ident(current_into.identifier, current_src.identifier)
            ProtoWrapper.__merge_versioned(current_into.as_string, current_src.as_string)

            ProtoWrapper.__nullity_matches(current_into.type, current_src.type, "Template param type")
            if current_into.type is not None:
                ProtoWrapper.__merge_typeinfo(current_into.type, current_src.type)

            if len(current_into.parameters) != len(current_src.parameters):
                raise Exception(
                    f"Failed to merge template details {current_into.parameters} and {current_src.parameters} because number of parameters aren't the same!")

            ProtoWrapper.__merge_template_params(current_into.parameters, current_src.parameters)

    @staticmethod
    def __merge_function_common(into: FunctionCommon, src: FunctionCommon):
        ProtoWrapper.__nullity_matches(into.return_type, src.return_type, "Function return type")
        if into.return_type is not None:
            ProtoWrapper.__merge_typeinfo(into.return_type, src.return_type)

        ProtoWrapper.__merge_versioned(into.storage_class, src.storage_class)
        ProtoWrapper.__merge_versioned(into.consteval_kind, src.consteval_kind)
        ProtoWrapper.__merge_versioned(into.inline_definition, src.inline_definition)

        ProtoWrapper.__nullity_matches(into.template_details, src.template_details, "Function template details")
        if into.template_details is not None:
            ProtoWrapper.__merge_template_params(into.template_details.parameters, src.template_details.parameters)

        ProtoWrapper.__merge_ident(into.identifier, src.identifier)
        ProtoWrapper.__merge_versioned(into.as_string, src.as_string)
        ProtoWrapper.__merge_versioned(into.content_hash, src.content_hash)

        if len(into.parameters) != len(src.parameters):
            raise Exception(f"Failed to merge function common (into: {into}, src: {src}) because the number of parameters aren't the same!")
        for i in range(len(into.parameters)):
            into_param = into.parameters[i]
            src_param = src.parameters[i]
            ProtoWrapper.__merge_ident(into_param.identifier, src_param.identifier)
            ProtoWrapper.__merge_versioned(into_param.as_string, src_param.as_string)
            ProtoWrapper.__merge_typeinfo(into_param.type_info, src_param.type_info)
            ProtoWrapper.__merge_versioned(into_param.default_value, src_param.default_value)
            ProtoWrapper.__merge_versioned(into_param.content_hash, src_param.content_hash)

    @staticmethod
    def __merge_versioned(into: object, src: object):
        if type(into) != type(src):
            raise Exception(f"Failed to merge versioned objects due to type mismatch: {into}, {src}")

        if isinstance(into, VersionedBool):
            if len(cast(VersionedBool, src).true_versions) != 0:
                into.true_versions.append(cast(VersionedBool, src).true_versions[0])
            else:
                into.false_versions.append(cast(VersionedBool, src).false_versions[0])
        elif isinstance(into, VersionedUint64List) or isinstance(into, VersionedStringList):
            into_map = {frozenset(obj.value): obj for obj in into.versions}
            src_map = {frozenset(obj.value): obj for obj in cast(VersionedUint64List | VersionedStringList, src).versions}
            for values in src_map.keys():
                into_value = into_map.get(values, None)
                if into_value is not None:
                    into_value.source_versions.append(src_map[values].source_versions[0])
                else:
                    into.versions.append(src_map[values])
        else:
            into_map = {obj.value: obj for obj in into.versions}
            src_map = {obj.value: obj for obj in src.versions}
            for value in src_map.values():
                into_value = into_map.get(value, None)
                if into_value is not None:
                    into_value.source_versions.append(src_map[value].source_versions[0])
                else:
                    into_value.versions.append(src_map[value])


    file_meta_re = re.compile(r"^(?P<id_hash>\d+)-(?P<content_hash>\d+)-(?P<path_hash>\d+)-(?P<OI>\d+)\.(?P<type>\w+)bin$")

class Merger:
    def __init__(self, version_file_list: list[str], input_dir: Path, output_dir: Path):
        # we could probably make this lazier
        self.wrappers: list[ProtoWrapper] = list(map(lambda file_path: ProtoWrapper(input_dir, file_path), version_file_list))
        self.output_dir = output_dir

    def merge(self) -> bool:
        try:
            num_files = len(self.wrappers)
            if num_files == 0:
                return True

            if num_files == 1:
                self.wrappers[0].write_obj(self.output_dir)
                return True

            target_wrapper = self.wrappers.pop(0)

            for wrapper in self.wrappers:
                target_wrapper.merge_with(wrapper)

            target_wrapper.write_obj(self.output_dir)
        except Exception:
            return False
        return True