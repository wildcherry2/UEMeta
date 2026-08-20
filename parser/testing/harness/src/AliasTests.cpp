#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "TemplateDetailsTestHelpers.hpp"
#include "TypeInfoHelpers.hpp"
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

    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME;
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

                if (declaration.metadata().identifier().file_path() != AliasSourcePath().string()) {
                    continue;
                }

                result.insert_or_assign(declaration.alias(), std::move(declaration));
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
        EXPECT_EQ(declaration.alias(), expected_alias);
        EXPECT_TRUE(declaration.has_aliased_type());
        UEMeta::Testing::ExpectTypeInfo(declaration.aliased_type(), expected_aliased_type);
        EXPECT_EQ(declaration.as_string(), expected_as_string);
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
