#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <simdjson.h>

namespace UEMeta {
    struct JsonTemplateParameter {
        std::string kind;
        std::string name;
        std::string type;
        bool is_parameter_pack{};
        std::vector<JsonTemplateParameter> parameters;
    };

    struct JsonVTableIndex {
        std::uint64_t index{};
        std::int64_t offset{};
    };

    struct JsonParameter {
        std::string name;
        std::string type;
        std::string declaration;
    };

    struct JsonFunction {
        std::string kind;
        std::string name;
        std::string qualified_name;
        std::string file;
        std::vector<std::string> scope;
        std::string return_type;
        std::string access;
        std::string storage_class;
        std::string ref_qualifier;
        std::string exception_spec;
        bool is_const{};
        bool is_volatile{};
        bool is_static{};
        bool is_virtual{};
        bool is_pure{};
        bool is_constexpr{};
        bool is_consteval{};
        bool is_inline{};
        bool is_deleted{};
        bool is_defaulted{};
        bool is_explicit{};
        std::optional<JsonVTableIndex> vtable_index;
        std::vector<JsonTemplateParameter> template_parameters;
        std::vector<JsonParameter> parameters;
    };

    struct JsonVariable {
        std::string name;
        std::string qualified_name;
        std::string file;
        std::vector<std::string> scope;
        std::string type;
        std::string declaration;
        std::string access;
        std::string storage_class;
        bool is_constexpr{};
        bool is_inline{};
        bool is_static_data_member{};
        bool is_thread_local{};
    };

    struct JsonEnumerator {
        std::string name;
        std::string value;
        std::string file;
        std::vector<std::string> scope;
    };

    struct JsonField {
        std::string name;
        std::string file;
        std::vector<std::string> scope;
        std::string type;
        std::string declaration;
        std::string access;
        bool is_mutable{};
        bool is_bitfield{};
        std::optional<std::uint64_t> bit_width;
        std::optional<std::uint64_t> offset_bits;
    };

    struct JsonBase {
        std::string type;
        std::string qualified_name;
        std::string access;
        bool is_virtual{};
        std::optional<std::int64_t> offset;
    };

    struct JsonDeclaration {
        std::string kind;
        std::string name;
        std::string qualified_name;
        std::string file;
        std::vector<std::string> scope;
        bool is_anonymous{};

        std::vector<JsonTemplateParameter> template_parameters;

        std::string underlying_type;
        bool is_scoped{};
        std::string scoped_kind;
        std::vector<JsonEnumerator> enumerators;

        bool is_complete_definition{};
        std::optional<std::uint64_t> size_bytes;
        std::optional<std::uint64_t> align_bytes;
        std::vector<JsonBase> bases;
        std::vector<JsonField> fields;
        std::vector<JsonVariable> static_variables;
        std::vector<JsonFunction> methods;
        std::vector<JsonDeclaration> nested;

        std::optional<JsonFunction> function;
        std::optional<JsonVariable> variable;

        std::string aliased_type;
    };

    template <typename Builder, typename Value>
    void AppendMember(Builder& builder, bool& needs_comma, const char* key, const Value& value) {
        if (needs_comma) {
            builder.append_comma();
        }
        builder.append_key_value(key, value);
        needs_comma = true;
    }

    template <typename Builder>
    void AppendKey(Builder& builder, bool& needs_comma, const char* key) {
        if (needs_comma) {
            builder.append_comma();
        }
        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        needs_comma = true;
    }

    template <typename Builder, typename Range>
    void AppendArrayMember(Builder& builder, bool& needs_comma, const char* key, const Range& range) {
        AppendKey(builder, needs_comma, key);
        builder.start_array();

        bool item_needs_comma = false;
        for (const auto& item : range) {
            if (item_needs_comma) {
                builder.append_comma();
            }
            builder.append(item);
            item_needs_comma = true;
        }

        builder.end_array();
    }

    template <typename Builder, typename Value>
    void AppendOptionalMember(Builder& builder, bool& needs_comma, const char* key, const std::optional<Value>& value) {
        if (value) {
            AppendMember(builder, needs_comma, key, *value);
        }
    }

    template <typename Builder>
    void AppendStringIfNotEmpty(Builder& builder, bool& needs_comma, const char* key, const std::string& value) {
        if (!value.empty()) {
            AppendMember(builder, needs_comma, key, value);
        }
    }

    template <typename Builder>
    void AppendBoolIfTrue(Builder& builder, bool& needs_comma, const char* key, bool value) {
        if (value) {
            AppendMember(builder, needs_comma, key, value);
        }
    }

    template <typename Builder>
    void AppendTemplateParameters(Builder& builder, bool& needs_comma, const std::vector<JsonTemplateParameter>& parameters) {
        if (!parameters.empty()) {
            AppendArrayMember(builder, needs_comma, "templateParameters", parameters);
        }
    }

    template <typename Builder>
    void AppendLocationMembers(Builder& builder, bool& needs_comma, const std::string& file,
                               const std::vector<std::string>& scope) {
        AppendMember(builder, needs_comma, "file", file);
        AppendArrayMember(builder, needs_comma, "scope", scope);
    }

    template <typename Builder>
    void AppendFunctionMembers(Builder& builder, bool& needs_comma, const JsonFunction& function,
                               const bool include_location = true) {
        AppendMember(builder, needs_comma, "functionKind", function.kind);
        AppendMember(builder, needs_comma, "name", function.name);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", function.qualified_name);
        if (include_location) {
            AppendLocationMembers(builder, needs_comma, function.file, function.scope);
        }
        AppendStringIfNotEmpty(builder, needs_comma, "returnType", function.return_type);
        AppendStringIfNotEmpty(builder, needs_comma, "access", function.access);
        AppendStringIfNotEmpty(builder, needs_comma, "storageClass", function.storage_class);
        AppendBoolIfTrue(builder, needs_comma, "isConst", function.is_const);
        AppendBoolIfTrue(builder, needs_comma, "isVolatile", function.is_volatile);
        AppendBoolIfTrue(builder, needs_comma, "isStatic", function.is_static);
        AppendBoolIfTrue(builder, needs_comma, "isVirtual", function.is_virtual);
        AppendBoolIfTrue(builder, needs_comma, "isPure", function.is_pure);
        AppendBoolIfTrue(builder, needs_comma, "isConstexpr", function.is_constexpr);
        AppendBoolIfTrue(builder, needs_comma, "isConsteval", function.is_consteval);
        AppendBoolIfTrue(builder, needs_comma, "isInline", function.is_inline);
        AppendBoolIfTrue(builder, needs_comma, "isDeleted", function.is_deleted);
        AppendBoolIfTrue(builder, needs_comma, "isDefaulted", function.is_defaulted);
        AppendBoolIfTrue(builder, needs_comma, "isExplicit", function.is_explicit);
        AppendStringIfNotEmpty(builder, needs_comma, "refQualifier", function.ref_qualifier);
        AppendStringIfNotEmpty(builder, needs_comma, "exceptionSpec", function.exception_spec);
        AppendTemplateParameters(builder, needs_comma, function.template_parameters);
        AppendArrayMember(builder, needs_comma, "parameters", function.parameters);

        if (function.vtable_index) {
            AppendKey(builder, needs_comma, "vtableIndex");
            builder.append(*function.vtable_index);
        }
    }

    template <typename Builder>
    void AppendVariableMembers(Builder& builder, bool& needs_comma, const JsonVariable& variable,
                               const bool include_location = true) {
        AppendMember(builder, needs_comma, "name", variable.name);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", variable.qualified_name);
        if (include_location) {
            AppendLocationMembers(builder, needs_comma, variable.file, variable.scope);
        }
        AppendMember(builder, needs_comma, "type", variable.type);
        AppendMember(builder, needs_comma, "declaration", variable.declaration);
        AppendStringIfNotEmpty(builder, needs_comma, "access", variable.access);
        AppendStringIfNotEmpty(builder, needs_comma, "storageClass", variable.storage_class);
        AppendBoolIfTrue(builder, needs_comma, "isConstexpr", variable.is_constexpr);
        AppendBoolIfTrue(builder, needs_comma, "isInline", variable.is_inline);
        AppendBoolIfTrue(builder, needs_comma, "isStaticDataMember", variable.is_static_data_member);
        AppendBoolIfTrue(builder, needs_comma, "isThreadLocal", variable.is_thread_local);
    }

    template <typename Builder>
    void AppendDeclarationArray(Builder& builder, const std::vector<JsonDeclaration>& declarations) {
        builder.start_array();

        bool needs_comma = false;
        for (const auto& declaration : declarations) {
            if (needs_comma) {
                builder.append_comma();
            }
            builder.append(declaration);
            needs_comma = true;
        }

        builder.end_array();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonTemplateParameter& parameter) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "kind", parameter.kind);
        AppendStringIfNotEmpty(builder, needs_comma, "name", parameter.name);
        AppendStringIfNotEmpty(builder, needs_comma, "type", parameter.type);
        AppendBoolIfTrue(builder, needs_comma, "isParameterPack", parameter.is_parameter_pack);
        if (!parameter.parameters.empty()) {
            AppendArrayMember(builder, needs_comma, "parameters", parameter.parameters);
        }
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonVTableIndex& vtable_index) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "index", vtable_index.index);
        AppendMember(builder, needs_comma, "offset", vtable_index.offset);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonParameter& parameter) {
        builder.start_object();
        bool needs_comma = false;
        AppendStringIfNotEmpty(builder, needs_comma, "name", parameter.name);
        AppendMember(builder, needs_comma, "type", parameter.type);
        AppendMember(builder, needs_comma, "declaration", parameter.declaration);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonFunction& function) {
        builder.start_object();
        bool needs_comma = false;
        AppendFunctionMembers(builder, needs_comma, function);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonVariable& variable) {
        builder.start_object();
        bool needs_comma = false;
        AppendVariableMembers(builder, needs_comma, variable);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonEnumerator& enumerator) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "name", enumerator.name);
        AppendMember(builder, needs_comma, "value", enumerator.value);
        AppendLocationMembers(builder, needs_comma, enumerator.file, enumerator.scope);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonField& field) {
        builder.start_object();
        bool needs_comma = false;
        AppendStringIfNotEmpty(builder, needs_comma, "name", field.name);
        AppendLocationMembers(builder, needs_comma, field.file, field.scope);
        AppendMember(builder, needs_comma, "type", field.type);
        AppendMember(builder, needs_comma, "declaration", field.declaration);
        AppendStringIfNotEmpty(builder, needs_comma, "access", field.access);
        AppendBoolIfTrue(builder, needs_comma, "isMutable", field.is_mutable);
        AppendBoolIfTrue(builder, needs_comma, "isBitfield", field.is_bitfield);
        AppendOptionalMember(builder, needs_comma, "bitWidth", field.bit_width);
        AppendOptionalMember(builder, needs_comma, "offsetBits", field.offset_bits);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonBase& base) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "type", base.type);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", base.qualified_name);
        AppendStringIfNotEmpty(builder, needs_comma, "access", base.access);
        AppendBoolIfTrue(builder, needs_comma, "isVirtual", base.is_virtual);
        AppendOptionalMember(builder, needs_comma, "offset", base.offset);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonDeclaration& declaration) {
        builder.start_object();
        bool needs_comma = false;

        AppendMember(builder, needs_comma, "kind", declaration.kind);
        const bool payload_has_identity = declaration.kind == "function" || declaration.kind == "variable";
        if (!payload_has_identity) {
            AppendStringIfNotEmpty(builder, needs_comma, "name", declaration.name);
            AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", declaration.qualified_name);
        }
        AppendLocationMembers(builder, needs_comma, declaration.file, declaration.scope);
        AppendBoolIfTrue(builder, needs_comma, "isAnonymous", declaration.is_anonymous);
        AppendTemplateParameters(builder, needs_comma, declaration.template_parameters);

        if (declaration.kind == "enum") {
            AppendStringIfNotEmpty(builder, needs_comma, "underlyingType", declaration.underlying_type);
            AppendBoolIfTrue(builder, needs_comma, "isScoped", declaration.is_scoped);
            AppendStringIfNotEmpty(builder, needs_comma, "scopedKind", declaration.scoped_kind);
            AppendArrayMember(builder, needs_comma, "enumerators", declaration.enumerators);
        } else if (declaration.kind == "class" || declaration.kind == "struct" || declaration.kind == "union") {
            AppendBoolIfTrue(builder, needs_comma, "isCompleteDefinition", declaration.is_complete_definition);
            AppendOptionalMember(builder, needs_comma, "sizeBytes", declaration.size_bytes);
            AppendOptionalMember(builder, needs_comma, "alignBytes", declaration.align_bytes);
            AppendArrayMember(builder, needs_comma, "bases", declaration.bases);
            AppendArrayMember(builder, needs_comma, "fields", declaration.fields);
            AppendArrayMember(builder, needs_comma, "staticVariables", declaration.static_variables);
            AppendArrayMember(builder, needs_comma, "methods", declaration.methods);
            AppendArrayMember(builder, needs_comma, "nested", declaration.nested);
        } else if (declaration.kind == "function" && declaration.function) {
            AppendFunctionMembers(builder, needs_comma, *declaration.function, false);
        } else if (declaration.kind == "variable" && declaration.variable) {
            AppendVariableMembers(builder, needs_comma, *declaration.variable, false);
        } else if (declaration.kind == "alias") {
            AppendMember(builder, needs_comma, "aliasedType", declaration.aliased_type);
        }

        builder.end_object();
    }
}
