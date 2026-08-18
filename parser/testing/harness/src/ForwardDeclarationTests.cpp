#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "EnumDetailsTestHelpers.hpp"
#include "TemplateDetailsTestHelpers.hpp"
#include "parser.pb.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::FORWARD_DECLARATION_KIND_CLASS;
    using ParseResult::FORWARD_DECLARATION_KIND_ENUM;
    using ParseResult::FORWARD_DECLARATION_KIND_STRUCT;
    using ParseResult::FORWARD_DECLARATION_KIND_UNION;
    using ParseResult::ForwardDeclarationKind;
    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION;
    using ParseResult::TEMPLATE_SPECIALIZATION_IMPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_NONE;
    using ParseResult::TemplateSpecializationKind;
    using ParseResult::TLForwardDeclaration;
    using ParseResult::TLRecordDeclaration;

    fs::path ForwardDeclarationSourcePath() {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "ForwardDeclarationTypes.hpp";
    }

    std::string QualifiedName(const std::string_view name) {
        return "UEMeta::Testing::Types::" + std::string{name};
    }

    const std::unordered_map<std::uint32_t, TLForwardDeclaration>& ForwardDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::uint32_t, TLForwardDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".fwdeclbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLForwardDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse forward-declaration output " << entry.path();
                    continue;
                }

                if (declaration.metadata().identifier().file_path()
                    != ForwardDeclarationSourcePath().string()) {
                    continue;
                }

                result.insert_or_assign(declaration.metadata().occurrence_index(), std::move(declaration));
            }

            return result;
        }();

        return declarations;
    }

    const TLForwardDeclaration& ForwardDeclarationAt(const std::uint32_t occurrence_index) {
        return ForwardDeclarations().at(occurrence_index);
    }

    std::string ConcreteTemplateKey(
        const std::string_view template_name,
        const TemplateSpecializationKind specialization_kind) {
        return std::string{template_name} + ":" + std::to_string(static_cast<int>(specialization_kind));
    }

    const std::unordered_map<std::string, TLRecordDeclaration>& ConcreteTemplateDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLRecordDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                const auto extension = entry.path().extension();
                if (!entry.is_regular_file()
                    || (extension != ".classbin" && extension != ".structbin" && extension != ".unionbin")) {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLRecordDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse concrete-template output " << entry.path();
                    continue;
                }

                if (declaration.metadata().identifier().file_path()
                    != ForwardDeclarationSourcePath().string()
                    || !declaration.has_template_details()) {
                    continue;
                }

                const auto key = ConcreteTemplateKey(
                    declaration.metadata().identifier().name(),
                    declaration.template_details().specialization_kind());
                result.insert_or_assign(key, std::move(declaration));
            }

            return result;
        }();

        return declarations;
    }

    void ExpectForwardDeclaration(
        const TLForwardDeclaration& declaration,
        const std::string_view expected_name,
        const std::uint32_t expected_occurrence_index,
        const ForwardDeclarationKind expected_kind,
        const std::string_view expected_as_string) {
        SCOPED_TRACE(expected_name);

        ASSERT_TRUE(declaration.has_metadata());
        UEMeta::Testing::ExpectDeclarationMetadata(
            declaration.metadata(),
            expected_name,
            QualifiedName(expected_name),
            ForwardDeclarationSourcePath(),
            expected_occurrence_index,
            false);
        EXPECT_EQ(declaration.kind(), expected_kind);
        EXPECT_EQ(declaration.as_string(), expected_as_string);
    }

    void ExpectPrimaryTemplate(
        const TLForwardDeclaration& declaration,
        const std::string_view template_name,
        const std::string_view first_parameter,
        const std::string_view second_parameter) {
        ASSERT_TRUE(declaration.has_template_details());
        const auto qualified_template_name = QualifiedName(template_name);
        UEMeta::Testing::ExpectTemplateDetails(
            declaration.template_details(),
            TEMPLATE_SPECIALIZATION_NONE,
            qualified_template_name,
            ForwardDeclarationSourcePath(),
            {
                {
                    first_parameter,
                    qualified_template_name + "::" + std::string{first_parameter},
                    TEMPLATE_PARAMETER_KIND_TYPENAME,
                    "typename " + std::string{first_parameter}
                },
                {
                    second_parameter,
                    qualified_template_name + "::" + std::string{second_parameter},
                    TEMPLATE_PARAMETER_KIND_TYPENAME,
                    "typename " + std::string{second_parameter}
                }
            },
            R"()");
    }

    void ExpectExplicitPartialSpecialization(
        const TLForwardDeclaration& declaration,
        const std::string_view template_name,
        const std::string_view parameter_name) {
        ASSERT_TRUE(declaration.has_template_details());
        const auto qualified_template_name = QualifiedName(template_name);
        UEMeta::Testing::ExpectTemplateDetails(
            declaration.template_details(),
            TEMPLATE_SPECIALIZATION_EXPLICIT,
            qualified_template_name,
            ForwardDeclarationSourcePath(),
            {
                {
                    parameter_name,
                    qualified_template_name + "<type-parameter-0-0, int>::" + std::string{parameter_name},
                    TEMPLATE_PARAMETER_KIND_TYPENAME,
                    "typename " + std::string{parameter_name}
                }
            },
            R"(type-parameter-0-0, int)");
    }

    void ExpectConcreteTemplateSpecialization(
        const std::string_view template_name,
        const std::string_view parameter_name,
        const TemplateSpecializationKind specialization_kind,
        const std::string_view arguments) {
        const auto& declaration = ConcreteTemplateDeclarations().at(
            ConcreteTemplateKey(template_name, specialization_kind));
        ASSERT_TRUE(declaration.has_template_details());

        const auto qualified_template_name = QualifiedName(template_name);
        UEMeta::Testing::ExpectTemplateDetails(
            declaration.template_details(),
            specialization_kind,
            qualified_template_name,
            ForwardDeclarationSourcePath(),
            {
                {
                    parameter_name,
                    qualified_template_name + "::" + std::string{parameter_name},
                    TEMPLATE_PARAMETER_KIND_TYPENAME,
                    "typename " + std::string{parameter_name}
                }
            },
            arguments);
    }
}

TEST(ForwardDeclarationTests, UntemplatedClass) {
    const auto& declaration = ForwardDeclarationAt(0);
    ExpectForwardDeclaration(
        declaration,
        "ForwardClass",
        0,
        FORWARD_DECLARATION_KIND_CLASS,
        R"(class ForwardClass)");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, UntemplatedStruct) {
    const auto& declaration = ForwardDeclarationAt(1);
    ExpectForwardDeclaration(
        declaration,
        "ForwardStruct",
        1,
        FORWARD_DECLARATION_KIND_STRUCT,
        R"(struct ForwardStruct)");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, UntemplatedUnion) {
    const auto& declaration = ForwardDeclarationAt(2);
    ExpectForwardDeclaration(
        declaration,
        "ForwardUnion",
        2,
        FORWARD_DECLARATION_KIND_UNION,
        R"(union ForwardUnion)");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, PrimaryClassTemplate) {
    const auto& declaration = ForwardDeclarationAt(3);
    ExpectForwardDeclaration(
        declaration,
        "ForwardClassTemplate",
        3,
        FORWARD_DECLARATION_KIND_CLASS,
        R"(template <typename ClassType, typename ClassValue> class ForwardClassTemplate)");
    ExpectPrimaryTemplate(declaration, "ForwardClassTemplate", "ClassType", "ClassValue");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, ExplicitClassTemplateSpecialization) {
    const auto& declaration = ForwardDeclarationAt(4);
    ExpectForwardDeclaration(
        declaration,
        "ForwardClassTemplate",
        4,
        FORWARD_DECLARATION_KIND_CLASS,
        R"(template <typename ClassType> class ForwardClassTemplate<ClassType, int>)");
    ExpectExplicitPartialSpecialization(declaration, "ForwardClassTemplate", "ClassType");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, PrimaryStructTemplate) {
    const auto& declaration = ForwardDeclarationAt(5);
    ExpectForwardDeclaration(
        declaration,
        "ForwardStructTemplate",
        5,
        FORWARD_DECLARATION_KIND_STRUCT,
        R"(template <typename StructType, typename StructValue> struct ForwardStructTemplate)");
    ExpectPrimaryTemplate(declaration, "ForwardStructTemplate", "StructType", "StructValue");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, ExplicitStructTemplateSpecialization) {
    const auto& declaration = ForwardDeclarationAt(6);
    ExpectForwardDeclaration(
        declaration,
        "ForwardStructTemplate",
        6,
        FORWARD_DECLARATION_KIND_STRUCT,
        R"(template <typename StructType> struct ForwardStructTemplate<StructType, int>)");
    ExpectExplicitPartialSpecialization(declaration, "ForwardStructTemplate", "StructType");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, PrimaryUnionTemplate) {
    const auto& declaration = ForwardDeclarationAt(7);
    ExpectForwardDeclaration(
        declaration,
        "ForwardUnionTemplate",
        7,
        FORWARD_DECLARATION_KIND_UNION,
        R"(template <typename UnionType, typename UnionValue> union ForwardUnionTemplate)");
    ExpectPrimaryTemplate(declaration, "ForwardUnionTemplate", "UnionType", "UnionValue");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, ExplicitUnionTemplateSpecialization) {
    const auto& declaration = ForwardDeclarationAt(8);
    ExpectForwardDeclaration(
        declaration,
        "ForwardUnionTemplate",
        8,
        FORWARD_DECLARATION_KIND_UNION,
        R"(template <typename UnionType> union ForwardUnionTemplate<UnionType, int>)");
    ExpectExplicitPartialSpecialization(declaration, "ForwardUnionTemplate", "UnionType");
    EXPECT_FALSE(declaration.has_enum_details());
}

TEST(ForwardDeclarationTests, UnscopedEnum) {
    const auto& declaration = ForwardDeclarationAt(9);
    ExpectForwardDeclaration(
        declaration,
        "ForwardUnscopedEnum",
        9,
        FORWARD_DECLARATION_KIND_ENUM,
        R"(enum ForwardUnscopedEnum : int)");
    ASSERT_TRUE(declaration.has_enum_details());
    UEMeta::Testing::ExpectEnumDetails(
        declaration.enum_details(),
        ParseResult::ENUM_SCOPE_UNSCOPED,
        "int");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(ForwardDeclarationTests, ScopedEnum) {
    const auto& declaration = ForwardDeclarationAt(10);
    ExpectForwardDeclaration(
        declaration,
        "ForwardScopedEnum",
        10,
        FORWARD_DECLARATION_KIND_ENUM,
        R"(enum class ForwardScopedEnum : int)");
    ASSERT_TRUE(declaration.has_enum_details());
    UEMeta::Testing::ExpectEnumDetails(
        declaration.enum_details(),
        ParseResult::ENUM_SCOPE_CLASS,
        "int");
    EXPECT_FALSE(declaration.has_template_details());
}

// These specialization kinds require complete template definitions. Their record-specific data is intentionally
// ignored; only the shared TemplateDetails message is validated here.
TEST(ForwardDeclarationTests, ImplicitClassTemplateSpecialization) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteClassTemplate",
        "ConcreteClassType",
        TEMPLATE_SPECIALIZATION_IMPLICIT,
        R"(char)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationClassTemplateDeclaration) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteClassTemplate",
        "ConcreteClassType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION,
        R"(short)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationClassTemplateDefinition) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteClassTemplate",
        "ConcreteClassType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION,
        R"(long)");
}

TEST(ForwardDeclarationTests, ImplicitStructTemplateSpecialization) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteStructTemplate",
        "ConcreteStructType",
        TEMPLATE_SPECIALIZATION_IMPLICIT,
        R"(float)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationStructTemplateDeclaration) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteStructTemplate",
        "ConcreteStructType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION,
        R"(double)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationStructTemplateDefinition) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteStructTemplate",
        "ConcreteStructType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION,
        R"(long double)");
}

TEST(ForwardDeclarationTests, ImplicitUnionTemplateSpecialization) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteUnionTemplate",
        "ConcreteUnionType",
        TEMPLATE_SPECIALIZATION_IMPLICIT,
        R"(signed char)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationUnionTemplateDeclaration) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteUnionTemplate",
        "ConcreteUnionType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION,
        R"(unsigned int)");
}

TEST(ForwardDeclarationTests, ExplicitInstantiationUnionTemplateDefinition) {
    ExpectConcreteTemplateSpecialization(
        "ConcreteUnionTemplate",
        "ConcreteUnionType",
        TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION,
        R"(unsigned long long)");
}
