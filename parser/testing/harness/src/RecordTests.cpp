#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "FunctionCommonTestHelpers.hpp"
#include "IdentifierTestHelpers.hpp"
#include "TemplateDetailsTestHelpers.hpp"
#include "TypeInfoHelpers.hpp"
#include "parser.pb.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::ACCESS_SPECIFIER_PRIVATE;
    using ParseResult::ACCESS_SPECIFIER_PROTECTED;
    using ParseResult::ACCESS_SPECIFIER_PUBLIC;
    using ParseResult::AccessSpecifier;
    using ParseResult::BaseSpecifier;
    using ParseResult::CONSTANT_EVALUATION_CONSTEXPR;
    using ParseResult::CONSTANT_EVALUATION_NONE;
    using ParseResult::ConstantEvaluationKind;
    using ParseResult::Field;
    using ParseResult::FUN_VAR_STORAGE_CLASS_UNSPECIFIED;
    using ParseResult::FUNCTION_DEFINITION_DEFAULTED;
    using ParseResult::FUNCTION_DEFINITION_DELETED;
    using ParseResult::FUNCTION_DEFINITION_NORMAL;
    using ParseResult::FUNCTION_KIND_DESTRUCTOR;
    using ParseResult::FUNCTION_KIND_MEMBER;
    using ParseResult::FUNCTION_VIRTUALITY_NONE;
    using ParseResult::FUNCTION_VIRTUALITY_PURE;
    using ParseResult::FUNCTION_VIRTUALITY_VIRTUAL;
    using ParseResult::FunctionDefinitionKind;
    using ParseResult::FunctionKind;
    using ParseResult::FunctionVirtuality;
    using ParseResult::MemberFunction;
    using ParseResult::RECORD_KIND_CLASS;
    using ParseResult::RECORD_KIND_STRUCT;
    using ParseResult::RECORD_KIND_UNION;
    using ParseResult::RecordKind;
    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION;
    using ParseResult::TEMPLATE_SPECIALIZATION_IMPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_NONE;
    using ParseResult::TemplateSpecializationKind;
    using ParseResult::TLRecordDeclaration;
    using UEMeta::Testing::ExpectedFunctionParameter;
    using UEMeta::Testing::ExpectedTypeInfo;

    enum class NestingShape { None, One, Double };
    enum class BaseShape { None, Single, Diamond };

    fs::path RecordSourcePath() { return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "RecordTypes.hpp"; }
    fs::path AliasSourcePath() { return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "AliasTypes.hpp"; }
    fs::path EnumSourcePath() { return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "EnumTypes.hpp"; }

    std::string QualifiedName(const std::string_view name) { return "UEMeta::Testing::Types::" + std::string{name}; }

    std::string TemplateArguments(const std::size_t template_parameter_count,
                                  const TemplateSpecializationKind specialization_kind) {
        if (specialization_kind == TEMPLATE_SPECIALIZATION_NONE) {
            return {};
        }

        const bool has_two_parameters = template_parameter_count == 2;
        switch (specialization_kind) {
        case TEMPLATE_SPECIALIZATION_IMPLICIT:
            return has_two_parameters ? "char, short" : "char";
        case TEMPLATE_SPECIALIZATION_EXPLICIT:
            return has_two_parameters ? "short, int" : "short";
        case TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION:
            return has_two_parameters ? "int, long" : "int";
        case TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION:
            return has_two_parameters ? "long, long long" : "long";
        case TEMPLATE_SPECIALIZATION_NONE:
            return {};
        }
        return {};
    }

    std::string TemplateArguments(const ParseResult::TemplateDetails& details) {
        std::string result;
        for (int index = 0; index < details.arguments_size(); ++index) {
            if (!result.empty()) {
                result += ", ";
            }
            result += details.arguments(index);
        }
        return result;
    }

    std::string RecordKey(const std::string_view qualified_name,
                          const std::optional<TemplateSpecializationKind> specialization_kind,
                          const std::string_view template_arguments = {}) {
        std::string result{qualified_name};
        result += ':';
        if (!specialization_kind) {
            result += "NoTemplate";
            return result;
        }

        result += std::to_string(static_cast<int>(*specialization_kind));
        result += ':';
        result += template_arguments;
        return result;
    }

    std::string RecordKey(const TLRecordDeclaration& declaration) {
        const auto& identifier = declaration.metadata().identifier();
        if (!declaration.has_template_details()) {
            return RecordKey(identifier.qualified_name(), std::nullopt);
        }

        const auto& details = declaration.template_details();
        if (!details.has_specialization_kind()) {
            ADD_FAILURE() << "Templated record has no specialization kind: " << identifier.qualified_name();
            return RecordKey(identifier.qualified_name(), std::nullopt);
        }

        return RecordKey(identifier.qualified_name(), details.specialization_kind(), TemplateArguments(details));
    }

    const std::unordered_map<std::string, TLRecordDeclaration>& RecordDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLRecordDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto extension = entry.path().extension();
                if (extension != ".classbin" && extension != ".structbin" && extension != ".unionbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLRecordDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse record output " << entry.path();
                    continue;
                }

                if (!declaration.has_metadata() || !declaration.metadata().has_identifier() ||
                    declaration.metadata().identifier().file_path() != RecordSourcePath().string()) {
                    continue;
                }

                const auto key = RecordKey(declaration);
                const auto [_, inserted] = result.emplace(key, std::move(declaration));
                if (!inserted) {
                    ADD_FAILURE() << "Duplicate record declaration key " << key;
                }
            }

            return result;
        }();

        return declarations;
    }

    const TLRecordDeclaration&
    FindRecord(const std::string_view qualified_name,
               const std::optional<TemplateSpecializationKind> specialization_kind = std::nullopt,
               const std::string_view template_arguments = {}) {
        const auto key = RecordKey(qualified_name, specialization_kind, template_arguments);
        const auto& declarations = RecordDeclarations();
        const auto found = declarations.find(key);
        EXPECT_NE(found, declarations.end()) << "No parser output for record " << key;
        if (found == declarations.end()) {
            static const TLRecordDeclaration missing;
            return missing;
        }
        return found->second;
    }

    void ExpectOptionalInt64(const bool has_value, const std::int64_t actual,
                             const std::optional<std::int64_t> expected) {
        EXPECT_EQ(has_value, expected.has_value());
        if (expected) {
            EXPECT_EQ(actual, *expected);
        }
    }

    void ExpectOptionalUInt64(const bool has_value, const std::uint64_t actual,
                              const std::optional<std::uint64_t> expected) {
        EXPECT_EQ(has_value, expected.has_value());
        if (expected) {
            EXPECT_EQ(actual, *expected);
        }
    }

    void ExpectRecordCore(const TLRecordDeclaration& declaration, const std::string_view expected_name,
                          const std::string_view expected_qualified_name, const RecordKind expected_kind,
                          const bool expected_is_complete_definition,
                          const std::optional<std::int64_t> expected_size_bytes,
                          const std::optional<std::int64_t> expected_align_bytes,
                          const std::size_t expected_field_count, const std::size_t expected_nested_count,
                          const std::size_t expected_base_count, const std::size_t expected_method_count) {
        SCOPED_TRACE(expected_qualified_name);

        ASSERT_TRUE(declaration.has_metadata());
        UEMeta::Testing::ExpectDeclarationMetadata(declaration.metadata(), expected_name, expected_qualified_name,
                                                   RecordSourcePath(), declaration.metadata().occurrence_index(),
                                                   false);

        EXPECT_EQ(declaration.kind(), expected_kind);
        ASSERT_TRUE(declaration.has_is_complete_definition());
        EXPECT_EQ(declaration.is_complete_definition(), expected_is_complete_definition);

        ExpectOptionalInt64(declaration.has_size_bytes(), declaration.size_bytes(), expected_size_bytes);
        ExpectOptionalInt64(declaration.has_align_bytes(), declaration.align_bytes(), expected_align_bytes);

        EXPECT_EQ(declaration.fields_size(), expected_field_count);
        EXPECT_EQ(declaration.nested_hashes_size(), expected_nested_count);
        EXPECT_EQ(declaration.bases_size(), expected_base_count);
        EXPECT_EQ(declaration.methods_size(), expected_method_count);
    }

    void ExpectRecordTemplateDetails(const TLRecordDeclaration& declaration, const std::string_view record_name,
                                     const std::size_t template_parameter_count,
                                     const TemplateSpecializationKind specialization_kind) {
        ASSERT_TRUE(declaration.has_template_details());
        const auto qualified_name = QualifiedName(record_name);
        const auto arguments = TemplateArguments(template_parameter_count, specialization_kind);

        if (template_parameter_count == 1) {
            UEMeta::Testing::ExpectTemplateDetails(
                declaration.template_details(), specialization_kind, qualified_name, RecordSourcePath(),
                {{"FirstType", qualified_name + "::FirstType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename FirstType"}},
                arguments);
            return;
        }

        ASSERT_EQ(template_parameter_count, 2);
        UEMeta::Testing::ExpectTemplateDetails(
            declaration.template_details(), specialization_kind, qualified_name, RecordSourcePath(),
            {{"FirstType", qualified_name + "::FirstType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename FirstType"},
             {"SecondType", qualified_name + "::SecondType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename SecondType"}},
            arguments);
    }

    std::string SpecializedQualifiedName(const std::string_view record_name, const std::size_t template_parameter_count,
                                         const TemplateSpecializationKind specialization_kind) {
        auto result = QualifiedName(record_name);
        const auto arguments = TemplateArguments(template_parameter_count, specialization_kind);
        if (!arguments.empty()) {
            result += '<';
            result += arguments;
            result += '>';
        }
        return result;
    }

    void ExpectNestedHash(const TLRecordDeclaration& declaration,
                          const std::string_view expected_nested_qualified_name) {
        ASSERT_EQ(declaration.nested_hashes_size(), 1);
        const auto expected_hash = std::hash<std::string>{}(std::string{expected_nested_qualified_name});
        EXPECT_NE(std::find(declaration.nested_hashes().begin(), declaration.nested_hashes().end(), expected_hash),
                  declaration.nested_hashes().end());
    }

    void ExpectNoNestedHashes(const TLRecordDeclaration& declaration) {
        EXPECT_TRUE(declaration.nested_hashes().empty());
    }

    void ExpectField(const TLRecordDeclaration& declaration, const std::string_view owner_qualified_name,
                     const std::string_view expected_name, const std::string_view expected_as_string,
                     const AccessSpecifier expected_access, const bool expected_is_mutable,
                     const bool expected_is_bitfield, const std::optional<std::uint64_t> expected_bit_width,
                     const std::optional<std::uint64_t> expected_offset_bits,
                     const std::optional<std::string_view> expected_default_value,
                     const ExpectedTypeInfo& expected_type_info) {
        SCOPED_TRACE(expected_name);

        const auto found = std::find_if(declaration.fields().begin(), declaration.fields().end(),
                                        [&](const Field& field) { return field.identifier().name() == expected_name; });
        ASSERT_NE(found, declaration.fields().end()) << "No field named " << expected_name;

        ASSERT_TRUE(found->has_identifier());
        UEMeta::Testing::ExpectIdentifier(found->identifier(), expected_name,
                                          std::string{owner_qualified_name} + "::" + std::string{expected_name},
                                          RecordSourcePath());

        EXPECT_EQ(found->as_string(), expected_as_string);
        EXPECT_EQ(found->access(), expected_access);
        EXPECT_EQ(found->is_mutable(), expected_is_mutable);
        EXPECT_EQ(found->is_bitfield(), expected_is_bitfield);
        ExpectOptionalUInt64(found->has_bit_width(), found->bit_width(), expected_bit_width);
        ExpectOptionalUInt64(found->has_offset_bits(), found->offset_bits(), expected_offset_bits);

        EXPECT_EQ(found->has_default_value(), expected_default_value.has_value());
        if (expected_default_value) {
            EXPECT_EQ(found->default_value(), *expected_default_value);
        }

        ASSERT_TRUE(found->has_type_info());
        UEMeta::Testing::ExpectTypeInfo(found->type_info(), expected_type_info);
    }

    void ExpectBase(const TLRecordDeclaration& declaration, const std::string_view expected_name,
                    const AccessSpecifier expected_access, const bool expected_is_virtual,
                    const std::optional<std::uint64_t> expected_offset) {
        SCOPED_TRACE(expected_name);

        const auto expected_qualified_name = QualifiedName(expected_name);
        const auto found =
            std::find_if(declaration.bases().begin(), declaration.bases().end(), [&](const BaseSpecifier& base) {
                return base.identifier().qualified_name() == expected_qualified_name;
            });
        ASSERT_NE(found, declaration.bases().end()) << "No base named " << expected_name;

        ASSERT_TRUE(found->has_identifier());
        UEMeta::Testing::ExpectIdentifier(found->identifier(), expected_name, expected_qualified_name,
                                          RecordSourcePath());

        EXPECT_EQ(found->access(), expected_access);
        ASSERT_TRUE(found->has_is_virtual());
        EXPECT_EQ(found->is_virtual(), expected_is_virtual);
        ExpectOptionalUInt64(found->has_offset(), found->offset(), expected_offset);
        EXPECT_EQ(found->as_string(), expected_name);
        ASSERT_TRUE(found->has_type_info());
        UEMeta::Testing::ExpectTypeInfo(
            found->type_info(),
            {expected_name, expected_name, false, RecordSourcePath()});
    }

    void ExpectMethod(const TLRecordDeclaration& declaration, const std::string_view owner_qualified_name,
                      const std::string_view expected_name, const FunctionKind expected_kind,
                      const std::string_view expected_return_type, const std::string_view expected_as_string,
                      const ConstantEvaluationKind expected_consteval_kind,
                      const std::optional<std::string_view> expected_inline_definition,
                      const FunctionDefinitionKind expected_definition_kind,
                      const std::initializer_list<ExpectedFunctionParameter> expected_parameters,
                      const AccessSpecifier expected_access, const bool expected_is_const,
                      const bool expected_is_volatile, const FunctionVirtuality expected_virtuality,
                      const bool expected_is_deleted, const std::optional<std::uint64_t> expected_vtable_index,
                      const std::optional<std::int64_t> expected_vtable_offset) {
        SCOPED_TRACE(expected_name);

        const auto found =
            std::find_if(declaration.methods().begin(), declaration.methods().end(), [&](const MemberFunction& method) {
                return method.common().identifier().name() == expected_name;
            });
        ASSERT_NE(found, declaration.methods().end()) << "No method named " << expected_name;
        ASSERT_TRUE(found->has_common());

        UEMeta::Testing::ExpectFunctionCommon(
            found->common(), expected_name, std::string{owner_qualified_name} + "::" + std::string{expected_name},
            RecordSourcePath(), expected_kind, expected_as_string,
            ExpectedTypeInfo{expected_return_type, expected_return_type},
            FUN_VAR_STORAGE_CLASS_UNSPECIFIED, expected_consteval_kind, std::nullopt, expected_inline_definition,
            std::nullopt, expected_parameters, expected_definition_kind);

        EXPECT_EQ(found->access(), expected_access);
        EXPECT_EQ(found->is_const(), expected_is_const);
        EXPECT_EQ(found->is_volatile(), expected_is_volatile);
        EXPECT_EQ(found->virtuality(), expected_virtuality);
        EXPECT_EQ(found->is_deleted(), expected_is_deleted);

        ASSERT_EQ(expected_vtable_index.has_value(), expected_vtable_offset.has_value());
        EXPECT_EQ(found->has_vtable_index(), expected_vtable_index.has_value());
        if (expected_vtable_index) {
            EXPECT_EQ(found->vtable_index().index(), *expected_vtable_index);
            EXPECT_EQ(found->vtable_index().offset(), *expected_vtable_offset);
        }
    }

    void ExpectMatrixFields(const TLRecordDeclaration& declaration, const std::string_view record_name,
                            const std::size_t template_parameter_count,
                            const TemplateSpecializationKind specialization_kind) {
        if (record_name != "ClassOneParameterNoNestedNoBaseRecord") {
            EXPECT_TRUE(declaration.fields().empty());
            return;
        }

        ASSERT_EQ(template_parameter_count, 1);
        const auto owner_qualified_name =
            SpecializedQualifiedName(record_name, template_parameter_count, specialization_kind);

        if (specialization_kind == TEMPLATE_SPECIALIZATION_NONE) {
            ExpectField(declaration, owner_qualified_name, "DependentField", "FirstType DependentField",
                        ACCESS_SPECIFIER_PUBLIC, false, false, std::nullopt, std::nullopt, std::nullopt,
                        {"FirstType", "FirstType", true});
            ExpectField(declaration, owner_qualified_name, "FixedField", "int FixedField", ACCESS_SPECIFIER_PUBLIC,
                        false, false, std::nullopt, std::nullopt, std::nullopt, {"int", "int"});
            return;
        }

        const auto dependent_type = [&]() -> std::string_view {
            switch (specialization_kind) {
            case TEMPLATE_SPECIALIZATION_IMPLICIT:
                return "char";
            case TEMPLATE_SPECIALIZATION_EXPLICIT:
                return "short";
            case TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION:
                return "int";
            case TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION:
                return "long";
            case TEMPLATE_SPECIALIZATION_NONE:
                return "FirstType";
            }
            return {};
        }();
        const std::uint64_t dependent_width = dependent_type == "char" ? 8 : dependent_type == "short" ? 16 : 32;

        ExpectField(declaration, owner_qualified_name, "DependentField",
                    std::string{dependent_type} + " DependentField", ACCESS_SPECIFIER_PUBLIC, false, false,
                    dependent_width, 0, std::nullopt, {dependent_type, dependent_type});
        ExpectField(declaration, owner_qualified_name, "FixedField", "int FixedField", ACCESS_SPECIFIER_PUBLIC, false,
                    false, 32, 32, std::nullopt, {"int", "int"});
    }

    void ExpectMatrixRecord(const std::string_view record_name, const RecordKind expected_kind,
                            const std::size_t template_parameter_count,
                            const TemplateSpecializationKind specialization_kind, const NestingShape nesting_shape,
                            const BaseShape base_shape) {
        const auto qualified_name = QualifiedName(record_name);
        const auto arguments = TemplateArguments(template_parameter_count, specialization_kind);
        const auto& declaration = FindRecord(qualified_name, specialization_kind, arguments);

        const bool has_matrix_fields = record_name == "ClassOneParameterNoNestedNoBaseRecord";
        const bool has_layout = !has_matrix_fields || specialization_kind != TEMPLATE_SPECIALIZATION_NONE;
        std::optional<std::int64_t> expected_size;
        std::optional<std::int64_t> expected_align;
        if (has_layout) {
            if (has_matrix_fields) {
                expected_size = 8;
                expected_align = 4;
            } else if (base_shape == BaseShape::None) {
                expected_size = 1;
                expected_align = 1;
            } else if (base_shape == BaseShape::Single) {
                expected_size = 24;
                expected_align = 8;
            } else {
                expected_size = 64;
                expected_align = 8;
            }
        }

        const std::size_t expected_field_count = has_matrix_fields ? 2 : 0;
        const std::size_t expected_nested_count = nesting_shape == NestingShape::None ? 0 : 1;
        const std::size_t expected_base_count = base_shape == BaseShape::None     ? 0
                                                : base_shape == BaseShape::Single ? 1
                                                                                  : 2;

        ExpectRecordCore(declaration, record_name, qualified_name, expected_kind, true, expected_size, expected_align,
                         expected_field_count, expected_nested_count, expected_base_count, 0);
        ExpectRecordTemplateDetails(declaration, record_name, template_parameter_count, specialization_kind);
        ExpectMatrixFields(declaration, record_name, template_parameter_count, specialization_kind);

        const auto specialized_qualified_name =
            SpecializedQualifiedName(record_name, template_parameter_count, specialization_kind);
        if (nesting_shape == NestingShape::None) {
            ExpectNoNestedHashes(declaration);
        } else if (nesting_shape == NestingShape::One) {
            ExpectNestedHash(declaration, specialized_qualified_name + "::NestedDecl");
        } else {
            ExpectNestedHash(declaration, specialized_qualified_name + "::IntermediateDecl");
        }

        const std::optional<std::uint64_t> first_base_offset =
            has_layout ? std::optional<std::uint64_t>{0} : std::nullopt;
        if (base_shape == BaseShape::Single) {
            ExpectBase(declaration, "BaseRecord", ACCESS_SPECIFIER_PUBLIC, false, first_base_offset);
        } else if (base_shape == BaseShape::Diamond) {
            ExpectBase(declaration, "DiamondLeftRecordBase", ACCESS_SPECIFIER_PUBLIC, false, first_base_offset);
            ExpectBase(declaration, "DiamondRightRecordBase", ACCESS_SPECIFIER_PROTECTED, false,
                       has_layout ? std::optional<std::uint64_t>{24} : std::nullopt);
        }
    }

    void ExpectSingleNestedRecord(const std::string_view outer_name, const std::size_t template_parameter_count,
                                  const TemplateSpecializationKind specialization_kind) {
        const auto outer_qualified_name =
            SpecializedQualifiedName(outer_name, template_parameter_count, specialization_kind);
        const auto nested_qualified_name = outer_qualified_name + "::NestedDecl";
        const auto& declaration = FindRecord(nested_qualified_name);

        constexpr bool has_layout = true;
        ExpectRecordCore(declaration, "NestedDecl", nested_qualified_name, RECORD_KIND_STRUCT, true,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt, 0, 0, 0, 0);
        EXPECT_FALSE(declaration.has_template_details());
        ExpectNoNestedHashes(declaration);
    }

    void ExpectIntermediateNestedRecord(const std::string_view outer_name, const std::size_t template_parameter_count,
                                        const TemplateSpecializationKind specialization_kind) {
        const auto outer_qualified_name =
            SpecializedQualifiedName(outer_name, template_parameter_count, specialization_kind);
        const auto intermediate_qualified_name = outer_qualified_name + "::IntermediateDecl";
        const auto& declaration = FindRecord(intermediate_qualified_name);

        constexpr bool has_layout = true;
        ExpectRecordCore(declaration, "IntermediateDecl", intermediate_qualified_name, RECORD_KIND_STRUCT, true,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt, 0, 1, 0, 0);
        EXPECT_FALSE(declaration.has_template_details());
        ExpectNestedHash(declaration, intermediate_qualified_name + "::AnotherDecl");
    }

    void ExpectInnermostNestedRecord(const std::string_view outer_name, const std::size_t template_parameter_count,
                                     const TemplateSpecializationKind specialization_kind) {
        const auto outer_qualified_name =
            SpecializedQualifiedName(outer_name, template_parameter_count, specialization_kind);
        const auto nested_qualified_name = outer_qualified_name + "::IntermediateDecl::AnotherDecl";
        const auto& declaration = FindRecord(nested_qualified_name);

        constexpr bool has_layout = true;
        ExpectRecordCore(declaration, "AnotherDecl", nested_qualified_name, RECORD_KIND_STRUCT, true,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt,
                         has_layout ? std::optional<std::int64_t>{1} : std::nullopt, 0, 0, 0, 0);
        EXPECT_FALSE(declaration.has_template_details());
        ExpectNoNestedHashes(declaration);
    }
} // namespace

TEST(RecordTests, BaseRecord) {
    const auto qualified_name = QualifiedName("BaseRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "BaseRecord", qualified_name, RECORD_KIND_CLASS, true, 24, 8, 3, 0, 0, 3);
    EXPECT_FALSE(declaration.has_template_details());
    ExpectNoNestedHashes(declaration);

    ExpectField(declaration, qualified_name, "field", "int field", ACCESS_SPECIFIER_PUBLIC, false, false, 32, 64, "0",
                {"int", "int"});
    ExpectField(declaration, qualified_name, "bfield", "Alpha bfield", ACCESS_SPECIFIER_PUBLIC, false, false, 8, 96,
                "{}", {"Alpha", "Alpha", false, AliasSourcePath()});
    ExpectField(declaration, qualified_name, "stage", "Stage stage", ACCESS_SPECIFIER_PUBLIC, false, false, 32, 128,
                "Stage::New", {"Stage", "Stage", false, EnumSourcePath()});

    ExpectMethod(declaration, qualified_name, "func", FUNCTION_KIND_MEMBER, "void", "void func()",
                 CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, false,
                 false, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "vfunc", FUNCTION_KIND_MEMBER, "void",
                 "virtual void vfunc(int a, bool b)", CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL,
                 {{"a", qualified_name + "::vfunc::a", {"int", "int"}, "", "int a"},
                  {"b", qualified_name + "::vfunc::b", {"_Bool", "_Bool"}, "", "bool b"}},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
    ExpectMethod(declaration, qualified_name, "~BaseRecord", FUNCTION_KIND_DESTRUCTOR, "void",
                 "virtual ~BaseRecord() noexcept = default", CONSTANT_EVALUATION_CONSTEXPR, "{\n}\n",
                 FUNCTION_DEFINITION_DEFAULTED, {}, ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL,
                 false, 1, 0);
}

TEST(RecordTests, DerivedRecord) {
    const auto qualified_name = QualifiedName("DerivedRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "DerivedRecord", qualified_name, RECORD_KIND_STRUCT, true, 32, 8, 1, 0, 1, 4);
    EXPECT_FALSE(declaration.has_template_details());
    ExpectNoNestedHashes(declaration);

    ExpectField(declaration, qualified_name, "nfield", "int nfield", ACCESS_SPECIFIER_PUBLIC, false, false, 32, 192,
                "1", {"int", "int"});
    ExpectBase(declaration, "BaseRecord", ACCESS_SPECIFIER_PUBLIC, false, 0);

    ExpectMethod(declaration, qualified_name, "vfunc", FUNCTION_KIND_MEMBER, "void",
                 "void vfunc(int a, bool b) override", CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL,
                 {{"a", qualified_name + "::vfunc::a", {"int", "int"}, "", "int a"},
                  {"b", qualified_name + "::vfunc::b", {"_Bool", "_Bool"}, "", "bool b"}},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
    ExpectMethod(declaration, qualified_name, "vfunc2", FUNCTION_KIND_MEMBER, "int", "virtual int vfunc2() const",
                 CONSTANT_EVALUATION_NONE, "{\n    return 4;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, true, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 2, 0);
    ExpectMethod(declaration, qualified_name, "sfunc", FUNCTION_KIND_MEMBER, "void", "void sfunc(char c)",
                 CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL,
                 {{"c", qualified_name + "::sfunc::c", {"char", "char"}, "", "char c"}},
                 ACCESS_SPECIFIER_PUBLIC, false, false,
                 FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "~DerivedRecord", FUNCTION_KIND_DESTRUCTOR, "void",
                 "~DerivedRecord() noexcept override = default", CONSTANT_EVALUATION_CONSTEXPR, "{\n}\n",
                 FUNCTION_DEFINITION_DEFAULTED, {}, ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL,
                 false, 1, 0);
}

TEST(RecordTests, DiamondRootRecordBase) {
    const auto qualified_name = QualifiedName("DiamondRootRecordBase");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "DiamondRootRecordBase", qualified_name, RECORD_KIND_STRUCT, true, 16, 8, 1, 0, 0, 1);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectField(declaration, qualified_name, "RootField", "int RootField", ACCESS_SPECIFIER_PUBLIC, false, false, 32,
                64, "1", {"int", "int"});
    ExpectMethod(declaration, qualified_name, "RootVirtual", FUNCTION_KIND_MEMBER, "int", "virtual int RootVirtual()",
                 CONSTANT_EVALUATION_NONE, "{\n    return this->RootField;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
}

TEST(RecordTests, DiamondLeftRecordBase) {
    const auto qualified_name = QualifiedName("DiamondLeftRecordBase");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "DiamondLeftRecordBase", qualified_name, RECORD_KIND_STRUCT, true, 40, 8, 1, 0, 1, 1);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectField(declaration, qualified_name, "LeftField", "int LeftField", ACCESS_SPECIFIER_PUBLIC, false, false, 32,
                128, "2", {"int", "int"});
    ExpectBase(declaration, "DiamondRootRecordBase", ACCESS_SPECIFIER_PUBLIC, true, 24);
    ExpectMethod(declaration, qualified_name, "LeftVirtual", FUNCTION_KIND_MEMBER, "int", "virtual int LeftVirtual()",
                 CONSTANT_EVALUATION_NONE, "{\n    return this->LeftField;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
}

TEST(RecordTests, DiamondRightRecordBase) {
    const auto qualified_name = QualifiedName("DiamondRightRecordBase");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "DiamondRightRecordBase", qualified_name, RECORD_KIND_STRUCT, true, 40, 8, 1, 0, 1,
                     1);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectField(declaration, qualified_name, "RightField", "int RightField", ACCESS_SPECIFIER_PUBLIC, false, false, 32,
                128, "3", {"int", "int"});
    ExpectBase(declaration, "DiamondRootRecordBase", ACCESS_SPECIFIER_PUBLIC, true, 24);
    ExpectMethod(declaration, qualified_name, "RightVirtual", FUNCTION_KIND_MEMBER, "int", "virtual int RightVirtual()",
                 CONSTANT_EVALUATION_NONE, "{\n    return this->RightField;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
}

TEST(RecordTests, DiamondMethodCoverageRecord) {
    const auto qualified_name = QualifiedName("DiamondMethodCoverageRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "DiamondMethodCoverageRecord", qualified_name, RECORD_KIND_STRUCT, true, 72, 8, 1, 0,
                     2, 3);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectField(declaration, qualified_name, "DirectField", "int DirectField", ACCESS_SPECIFIER_PUBLIC, false, false,
                32, 384, "4", {"int", "int"});
    ExpectBase(declaration, "DiamondLeftRecordBase", ACCESS_SPECIFIER_PUBLIC, false, 0);
    ExpectBase(declaration, "DiamondRightRecordBase", ACCESS_SPECIFIER_PROTECTED, false, 24);

    ExpectMethod(declaration, qualified_name, "LeftVirtual", FUNCTION_KIND_MEMBER, "int", "int LeftVirtual() override",
                 CONSTANT_EVALUATION_NONE, "{\n    return 4;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
    ExpectMethod(declaration, qualified_name, "RightVirtual", FUNCTION_KIND_MEMBER, "int",
                 "int RightVirtual() override", CONSTANT_EVALUATION_NONE, "{\n    return 5;\n}\n",
                 FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL,
                 false, 0, 24);
    ExpectMethod(declaration, qualified_name, "OwnVirtual", FUNCTION_KIND_MEMBER, "int", "virtual int OwnVirtual()",
                 CONSTANT_EVALUATION_NONE, "{\n    return 6;\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 1, 0);
}

TEST(RecordTests, MethodCoverageRecord) {
    const auto qualified_name = QualifiedName("MethodCoverageRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "MethodCoverageRecord", qualified_name, RECORD_KIND_CLASS, false, 8, 8, 0, 0, 0, 10);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectMethod(declaration, qualified_name, "PublicPlain", FUNCTION_KIND_MEMBER, "void", "void PublicPlain()",
                 CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, false,
                 false, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PublicConst", FUNCTION_KIND_MEMBER, "void", "void PublicConst() const",
                 CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, true,
                 false, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PublicVolatile", FUNCTION_KIND_MEMBER, "void",
                 "void PublicVolatile() volatile", CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, true, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PublicConstVolatile", FUNCTION_KIND_MEMBER, "void",
                 "void PublicConstVolatile() const volatile", CONSTANT_EVALUATION_NONE, "{\n}\n",
                 FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, true, true, FUNCTION_VIRTUALITY_NONE, false,
                 std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PublicVirtual", FUNCTION_KIND_MEMBER, "void",
                 "virtual void PublicVirtual()", CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 0, 0);
    ExpectMethod(declaration, qualified_name, "PublicPureVirtual", FUNCTION_KIND_MEMBER, "void",
                 "virtual void PublicPureVirtual() = 0", CONSTANT_EVALUATION_NONE, std::nullopt,
                 FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_PURE, false,
                 1, 0);
    ExpectMethod(declaration, qualified_name, "PublicDeleted", FUNCTION_KIND_MEMBER, "void",
                 "void PublicDeleted() = delete", CONSTANT_EVALUATION_NONE, std::nullopt, FUNCTION_DEFINITION_DELETED,
                 {}, ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_NONE, true, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PublicDeclaredOnly", FUNCTION_KIND_MEMBER, "void",
                 "void PublicDeclaredOnly()", CONSTANT_EVALUATION_NONE, std::nullopt, FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PUBLIC, false, false, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PrivatePlain", FUNCTION_KIND_MEMBER, "void", "void PrivatePlain()",
                 CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {}, ACCESS_SPECIFIER_PRIVATE, false,
                 false, FUNCTION_VIRTUALITY_NONE, false, std::nullopt, std::nullopt);
    ExpectMethod(declaration, qualified_name, "PrivateVirtual", FUNCTION_KIND_MEMBER, "void",
                 "virtual void PrivateVirtual()", CONSTANT_EVALUATION_NONE, "{\n}\n", FUNCTION_DEFINITION_NORMAL, {},
                 ACCESS_SPECIFIER_PRIVATE, false, false, FUNCTION_VIRTUALITY_VIRTUAL, false, 2, 0);
}

TEST(RecordTests, FieldCoverageRecord) {
    const auto qualified_name = QualifiedName("FieldCoverageRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(declaration, "FieldCoverageRecord", qualified_name, RECORD_KIND_CLASS, true, 32, 8, 7, 0, 0, 0);
    EXPECT_FALSE(declaration.has_template_details());

    ExpectField(declaration, qualified_name, "PublicPlain", "int PublicPlain", ACCESS_SPECIFIER_PUBLIC, false, false,
                32, 0, std::nullopt, {"int", "int"});
    ExpectField(declaration, qualified_name, "PublicMutablePointer", "mutable const int *PublicMutablePointer",
                ACCESS_SPECIFIER_PUBLIC, true, false, 64, 64, "nullptr", {"const int *", "int"});
    ExpectField(declaration, qualified_name, "PublicBitField", "unsigned int PublicBitField : 3",
                ACCESS_SPECIFIER_PUBLIC, false, true, 3, 128, std::nullopt, {"unsigned int", "unsigned int"});
    ExpectField(declaration, qualified_name, "PublicDefaultBitField", "unsigned int PublicDefaultBitField : 5",
                ACCESS_SPECIFIER_PUBLIC, false, true, 5, 131, "7", {"unsigned int", "unsigned int"});
    ExpectField(declaration, qualified_name, "PrivatePlain", "long PrivatePlain", ACCESS_SPECIFIER_PRIVATE, false,
                false, 32, 160, "19", {"long", "long"});
    ExpectField(declaration, qualified_name, "PrivateMutable", "mutable int PrivateMutable", ACCESS_SPECIFIER_PRIVATE,
                true, false, 32, 192, std::nullopt, {"int", "int"});
    ExpectField(declaration, qualified_name, "PrivateArray", "short PrivateArray[2]", ACCESS_SPECIFIER_PRIVATE, false,
                false, 32, 224, "{}", {"short[2]", "short"});
}

TEST(RecordTests, ClassOneParameterNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterNoNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterNoNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterNoNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterNoNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterNoNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterNoNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterNoNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterNoNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterNoNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterNoNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterNoNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterNoNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterNoNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterOneNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterOneNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterOneNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedSingleBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedSingleBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedDiamondBasesRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassOneParameterOneNestedDiamondBasesRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedSingleBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedNoBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedNoBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedNoBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersNoNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersNoNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersNoNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersNoNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersNoNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersNoNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersNoNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersNoNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersNoNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersNoNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedNoBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedSingleBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedSingleBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedDiamondBasesRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("ClassTwoParametersOneNestedDiamondBasesRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedSingleBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_CLASS, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ClassTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("ClassTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructOneParameterNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructOneParameterNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructOneParameterNoNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterNoNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterNoNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterNoNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, StructOneParameterNoNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, StructOneParameterNoNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterNoNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterNoNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterNoNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterNoNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Single);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedSingleBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterOneNestedSingleBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedSingleBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedDiamondBasesRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructOneParameterOneNestedDiamondBasesRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedSingleBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructOneParameterDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructOneParameterDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructOneParameterDoublyNestedDiamondBasesRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersNoNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersNoNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersNoNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersNoNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersNoNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersNoNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersNoNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersNoNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersNoNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersNoNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::None, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersNoNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersNoNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersNoNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersOneNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedSingleBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersOneNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersOneNestedSingleBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedSingleBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedDiamondBasesRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersOneNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersOneNestedDiamondBasesRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("StructTwoParametersOneNestedDiamondBasesRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedSingleBaseRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Single);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedSingleBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedSingleBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Primary) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_NONE, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Implicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Explicit) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", RECORD_KIND_STRUCT, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::Diamond);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, StructTwoParametersDoublyNestedDiamondBasesRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("StructTwoParametersDoublyNestedDiamondBasesRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionOneParameterNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionOneParameterNoNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionOneParameterNoNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionOneParameterNoNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionOneParameterNoNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionOneParameterNoNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionOneParameterOneNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("UnionOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionOneParameterOneNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("UnionOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionOneParameterOneNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("UnionOneParameterOneNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionOneParameterOneNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("UnionOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionOneParameterOneNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("UnionOneParameterOneNestedNoBaseRecord", 1,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 1, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionOneParameterDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 1,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionOneParameterDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionOneParameterDoublyNestedNoBaseRecord", 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionTwoParametersNoNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionTwoParametersNoNestedNoBaseRecord", RECORD_KIND_UNION, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersNoNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionTwoParametersNoNestedNoBaseRecord", RECORD_KIND_UNION, 2, TEMPLATE_SPECIALIZATION_IMPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersNoNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionTwoParametersNoNestedNoBaseRecord", RECORD_KIND_UNION, 2, TEMPLATE_SPECIALIZATION_EXPLICIT,
                       NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionTwoParametersNoNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersNoNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionTwoParametersNoNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::None, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionTwoParametersOneNestedNoBaseRecord", RECORD_KIND_UNION, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Primary_NestedDecl) {
    ExpectSingleNestedRecord("UnionTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionTwoParametersOneNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Implicit_NestedDecl) {
    ExpectSingleNestedRecord("UnionTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionTwoParametersOneNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_Explicit_NestedDecl) {
    ExpectSingleNestedRecord("UnionTwoParametersOneNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionTwoParametersOneNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDeclaration_NestedDecl) {
    ExpectSingleNestedRecord("UnionTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionTwoParametersOneNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::One, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersOneNestedNoBaseRecord_ExplicitInstantiationDefinition_NestedDecl) {
    ExpectSingleNestedRecord("UnionTwoParametersOneNestedNoBaseRecord", 2,
                             TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Primary) {
    ExpectMatrixRecord("UnionTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 2, TEMPLATE_SPECIALIZATION_NONE,
                       NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Primary_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Primary_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Implicit) {
    ExpectMatrixRecord("UnionTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_IMPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Implicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Implicit_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Explicit) {
    ExpectMatrixRecord("UnionTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT, NestingShape::Double, BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Explicit_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_Explicit_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration) {
    ExpectMatrixRecord("UnionTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDeclaration_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition) {
    ExpectMatrixRecord("UnionTwoParametersDoublyNestedNoBaseRecord", RECORD_KIND_UNION, 2,
                       TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION, NestingShape::Double,
                       BaseShape::None);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_IntermediateDecl) {
    ExpectIntermediateNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2,
                                   TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, UnionTwoParametersDoublyNestedNoBaseRecord_ExplicitInstantiationDefinition_AnotherDecl) {
    ExpectInnermostNestedRecord("UnionTwoParametersDoublyNestedNoBaseRecord", 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(RecordTests, ConcreteTemplateBaseTypeInfo) {
    const auto qualified_name = QualifiedName("TypeInfoTemplatedBaseRecord");
    const auto& declaration = FindRecord(qualified_name);
    ExpectRecordCore(
        declaration,
        "TypeInfoTemplatedBaseRecord",
        qualified_name,
        RECORD_KIND_STRUCT,
        true,
        8,
        4,
        0,
        0,
        1,
        0);
    EXPECT_FALSE(declaration.has_template_details());
    ExpectNoNestedHashes(declaration);

    ASSERT_EQ(declaration.bases_size(), 1);
    const auto& base = declaration.bases(0);
    EXPECT_EQ(base.access(), ACCESS_SPECIFIER_PUBLIC);
    ASSERT_TRUE(base.has_is_virtual());
    EXPECT_FALSE(base.is_virtual());
    ASSERT_TRUE(base.has_offset());
    EXPECT_EQ(base.offset(), 0);
    EXPECT_EQ(base.as_string(), "ClassOneParameterNoNestedNoBaseRecord<char>");
    ASSERT_TRUE(base.has_type_info());
    UEMeta::Testing::ExpectTypeInfo(
        base.type_info(),
        {
            "ClassOneParameterNoNestedNoBaseRecord<char>",
            "ClassOneParameterNoNestedNoBaseRecord<char>",
            false,
            RecordSourcePath()
        });
}

TEST(RecordTests, CoversEveryRecordDeclarationInRecordTypes) {
    // 210 outer matrix records + 210 explicitly checked nested records + 9 support/layout records.
    EXPECT_EQ(RecordDeclarations().size(), 429);
}
