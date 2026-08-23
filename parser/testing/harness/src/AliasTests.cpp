#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "TemplateDetailsTestHelpers.hpp"
#include "TypeInfoHelpers.hpp"
#include "VersionedProtoTestHelpers.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::TEMPLATE_PARAMETER_KIND_NON_TYPE;
    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME;
    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE;
    using ParseResult::TEMPLATE_SPECIALIZATION_NONE;
    using ParseResult::TLAliasDeclaration;
    using UEMeta::Testing::ExpectedTypeInfo;

    fs::path AliasSourcePath() {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "AliasTypes.hpp";
    }

    std::string QualifiedName(const std::string_view name) {
        return "UEMeta::Testing::Types::" + std::string{name};
    }

    const std::unordered_map<std::string, TLAliasDeclaration>& AliasDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLAliasDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".aliasbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLAliasDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse alias output " << entry.path();
                    continue;
                }

                if (UEMeta::Testing::VersionedValue(declaration.metadata().identifier().file_path())
                    != AliasSourcePath().string()) {
                    continue;
                }

                result.insert_or_assign(
                    UEMeta::Testing::VersionedValue(declaration.alias()), std::move(declaration));
            }

            return result;
        }();

        return declarations;
    }

    const TLAliasDeclaration& ExpectAlias(
        const std::string_view expected_alias,
        const std::uint32_t expected_occurrence_index,
        const ExpectedTypeInfo& expected_aliased_type,
        const std::string_view expected_as_string) {
        SCOPED_TRACE(expected_alias);

        const auto& declarations = AliasDeclarations();
        const auto found = declarations.find(std::string{expected_alias});
        EXPECT_NE(found, declarations.end()) << "No parser output for alias " << expected_alias;
        if (found == declarations.end()) {
            static const TLAliasDeclaration missing;
            return missing;
        }

        const auto& declaration = found->second;
        EXPECT_TRUE(declaration.has_metadata());
        UEMeta::Testing::ExpectDeclarationMetadata(
            declaration.metadata(),
            expected_alias,
            QualifiedName(expected_alias),
            AliasSourcePath(),
            expected_occurrence_index,
            false);
        EXPECT_EQ(UEMeta::Testing::VersionedValue(declaration.alias()), expected_alias);
        EXPECT_TRUE(declaration.has_aliased_type());
        UEMeta::Testing::ExpectTypeInfo(declaration.aliased_type(), expected_aliased_type);
        EXPECT_EQ(UEMeta::Testing::VersionedValue(declaration.as_string()), expected_as_string);
        return declaration;
    }
}

TEST(AliasTests, UsingDeclaration) {
    const auto& declaration = ExpectAlias(
        "Alpha",
        1,
        {"Beta", "Beta", false, AliasSourcePath()},
        R"(using Alpha = Beta)");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(AliasTests, TemplatedUsingDeclaration) {
    const auto& declaration = ExpectAlias(
        "TemplatedAlpha",
        2,
        {"AliasType", "AliasType", true},
        R"(template <typename AliasType> using TemplatedAlpha = AliasType)");

    ASSERT_TRUE(declaration.has_template_details());
    const auto qualified_alias_name = QualifiedName("TemplatedAlpha");
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        qualified_alias_name,
        AliasSourcePath(),
        {
            {
                "AliasType",
                QualifiedName("AliasType"),
                TEMPLATE_PARAMETER_KIND_TYPENAME,
                "typename AliasType"
            }
        },
        R"()");
}

TEST(AliasTests, TypedefDeclaration) {
    const auto& declaration = ExpectAlias(
        "LegacyAlpha",
        3,
        {"Beta", "Beta", false, AliasSourcePath()},
        R"(typedef Beta LegacyAlpha)");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(AliasTests, WrappedDeclaredType) {
    const auto& declaration = ExpectAlias(
        "WrappedAlpha",
        6,
        {"const Beta *const[2]", "Beta", false, AliasSourcePath()},
        R"(using WrappedAlpha = const Beta *const[2])");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(AliasTests, ReferenceDeclaredType) {
    const auto& declaration = ExpectAlias(
        "ReferenceAlpha",
        7,
        {"const Beta &", "Beta", false, AliasSourcePath()},
        R"(using ReferenceAlpha = const Beta &)");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(AliasTests, WrappedDependentType) {
    const auto& declaration = ExpectAlias(
        "TemplatedPointerAlpha",
        8,
        {"const AliasType *", "AliasType", true},
        R"(template <typename AliasType> using TemplatedPointerAlpha = const AliasType *)");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("TemplatedPointerAlpha"),
        AliasSourcePath(),
        {{"AliasType", QualifiedName("AliasType"), TEMPLATE_PARAMETER_KIND_TYPENAME, "typename AliasType"}},
        R"()");
}

TEST(AliasTests, DependentMemberType) {
    const auto& declaration = ExpectAlias(
        "DependentMemberAlpha",
        9,
        {"typename OwnerType::type", "typename OwnerType::type", true},
        R"(template <typename OwnerType> using DependentMemberAlpha = typename OwnerType::type)");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("DependentMemberAlpha"),
        AliasSourcePath(),
        {{"OwnerType", QualifiedName("OwnerType"), TEMPLATE_PARAMETER_KIND_TYPENAME, "typename OwnerType"}},
        R"()");
}

TEST(AliasTests, DeclaredTypeDefault) {
    const auto& declaration = ExpectAlias(
        "DefaultedTemplatedAlpha",
        10,
        {"AliasType", "AliasType", true},
        R"(template <typename AliasType = const Beta *> using DefaultedTemplatedAlpha = AliasType)");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("DefaultedTemplatedAlpha"),
        AliasSourcePath(),
        {{
            "AliasType",
            QualifiedName("AliasType"),
            TEMPLATE_PARAMETER_KIND_TYPENAME,
            "typename AliasType = const Beta *",
            std::nullopt,
            std::nullopt,
            std::nullopt,
            ExpectedTypeInfo{"const Beta *", "Beta", false, AliasSourcePath()}
        }},
        R"()");
}

TEST(AliasTests, DependentTypeDefault) {
    const auto& declaration = ExpectAlias(
        "DependentDefaultAlpha",
        11,
        {"AliasType", "AliasType", true},
        R"(template <typename OwnerType, typename AliasType = typename OwnerType::type> using DependentDefaultAlpha = AliasType)");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("DependentDefaultAlpha"),
        AliasSourcePath(),
        {
            {"OwnerType", QualifiedName("OwnerType"), TEMPLATE_PARAMETER_KIND_TYPENAME, "typename OwnerType"},
            {
                "AliasType",
                QualifiedName("AliasType"),
                TEMPLATE_PARAMETER_KIND_TYPENAME,
                "typename AliasType = typename OwnerType::type",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                ExpectedTypeInfo{"typename OwnerType::type", "typename OwnerType::type", true}
            }
        },
        R"()");
}

TEST(AliasTests, NonTypeDefaultUsesValue) {
    const auto& declaration = ExpectAlias(
        "SizedAlpha",
        12,
        {"Beta[Size]", "Beta", true},
        R"(template <int Size = 2> using SizedAlpha = Beta[Size])");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("SizedAlpha"),
        AliasSourcePath(),
        {{
            "Size",
            QualifiedName("Size"),
            TEMPLATE_PARAMETER_KIND_NON_TYPE,
            "int Size = 2",
            "int",
            std::nullopt,
            "2"
        }},
        R"()");
}

TEST(AliasTests, TemplateDefaultUsesDefaultType) {
    const auto& declaration = ExpectAlias(
        "TemplateDefaultedAlpha",
        13,
        {"TemplateType<Beta>", "TemplateType<Beta>", true},
        R"(template <template <typename TemplateValue> typename TemplateType = AliasTemplate> using TemplateDefaultedAlpha = TemplateType<Beta>)");

    ASSERT_TRUE(declaration.has_template_details());
    UEMeta::Testing::ExpectTemplateDetails(
        declaration.template_details(),
        TEMPLATE_SPECIALIZATION_NONE,
        QualifiedName("TemplateDefaultedAlpha"),
        AliasSourcePath(),
        {{
            "TemplateType",
            QualifiedName("TemplateType"),
            TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE,
            "template <typename TemplateValue> typename TemplateType = AliasTemplate",
            std::nullopt,
            std::nullopt,
            std::nullopt,
            ExpectedTypeInfo{"AliasTemplate", "AliasTemplate", false, AliasSourcePath()},
            {{
                "TemplateValue",
                QualifiedName("TemplateValue"),
                TEMPLATE_PARAMETER_KIND_TYPENAME,
                "typename TemplateValue"
            }}
        }},
        R"()");
}

TEST(AliasTests, ConcreteTemplateSpecialization) {
    const auto& declaration = ExpectAlias(
        "SpecializedAlpha",
        14,
        {"AliasTemplate<int>", "AliasTemplate<int>", false, AliasSourcePath()},
        R"(using SpecializedAlpha = AliasTemplate<int>)");
    EXPECT_FALSE(declaration.has_template_details());
}

TEST(AliasCoverageTests, AccountsForEveryTargetDeclaration) {
    EXPECT_EQ(AliasDeclarations().size(), 12);
}
