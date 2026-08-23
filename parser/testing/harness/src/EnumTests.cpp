#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "EnumDetailsTestHelpers.hpp"
#include "IdentifierTestHelpers.hpp"
#include "VersionedProtoTestHelpers.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::ENUM_SCOPE_CLASS;
    using ParseResult::ENUM_SCOPE_STRUCT;
    using ParseResult::ENUM_SCOPE_UNSCOPED;
    using ParseResult::EnumScope;
    using ParseResult::TLEnumDeclaration;

    struct ExpectedEnumerator {
        std::string_view identifier;
        std::string_view value;
    };

    fs::path EnumSourcePath() {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "EnumTypes.hpp";
    }

    std::string QualifiedEnumName(const std::string_view enum_name) {
        return "UEMeta::Testing::Types::" + std::string{enum_name};
    }

    std::string AnonymousEnumQualifiedName() {
        auto source_path = EnumSourcePath();
        source_path.make_preferred();
        return "UEMeta::Testing::Types::(unnamed enum at " + source_path.string() + ":41:5)";
    }

    std::string QualifiedEnumeratorName(
        const EnumScope enum_scope,
        const std::string_view qualified_enum_name,
        const std::string_view enumerator_name) {
        if (enum_scope != ENUM_SCOPE_UNSCOPED) {
            return std::string{qualified_enum_name} + "::" + std::string{enumerator_name};
        }

        const auto enum_name_begin = qualified_enum_name.rfind("::");
        return std::string{qualified_enum_name.substr(0, enum_name_begin + 2)} + std::string{enumerator_name};
    }

    const std::unordered_map<std::string, TLEnumDeclaration>& EnumDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLEnumDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".enumbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLEnumDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse enum output " << entry.path();
                    continue;
                }

                const auto& identifier = declaration.metadata().identifier();
                result.insert_or_assign(identifier.name(), std::move(declaration));
            }

            return result;
        }();

        return declarations;
    }

    void ExpectEnum(
        const std::string_view enum_name,
        const std::uint32_t occurrence_index,
        const EnumScope scope,
        const std::initializer_list<ExpectedEnumerator> expected_enumerators,
        const std::string_view underlying_type = {}) {
        SCOPED_TRACE(enum_name);

        const auto& declarations = EnumDeclarations();
        const auto found = declarations.find(std::string{enum_name});
        ASSERT_NE(found, declarations.end()) << "No parser output for enum " << enum_name;

        const auto& declaration = found->second;
        const auto qualified_enum_name = QualifiedEnumName(enum_name);
        UEMeta::Testing::ExpectDeclarationMetadata(
            declaration.metadata(),
            enum_name,
            qualified_enum_name,
            EnumSourcePath(),
            occurrence_index,
            false);

        ASSERT_TRUE(declaration.has_details());
        UEMeta::Testing::ExpectEnumDetails(declaration.details(), scope, underlying_type);

        ASSERT_EQ(declaration.enumerators_size(), expected_enumerators.size());
        int index = 0;
        for (const auto& expected : expected_enumerators) {
            const auto& enumerator = declaration.enumerators(index++);
            ASSERT_TRUE(enumerator.has_identifier());
            UEMeta::Testing::ExpectIdentifier(
                enumerator.identifier(),
                expected.identifier,
                QualifiedEnumeratorName(scope, qualified_enum_name, expected.identifier),
                EnumSourcePath());
            EXPECT_EQ(UEMeta::Testing::VersionedValue(enumerator.value()), expected.value);
        }
    }
}

TEST(EnumTests, NaturalUnscopedEmpty) {
    ExpectEnum("NaturalUnscopedEmpty", 1, ENUM_SCOPE_UNSCOPED, {});
}

TEST(EnumTests, NaturalUnscopedSingle) {
    ExpectEnum("NaturalUnscopedSingle", 2, ENUM_SCOPE_UNSCOPED, {{"NaturalUnscopedSingleItem", "0"}});
}

TEST(EnumTests, NaturalUnscopedPair) {
    ExpectEnum("NaturalUnscopedPair", 3, ENUM_SCOPE_UNSCOPED, {
        {"NaturalUnscopedPairFirst", "0"},
        {"NaturalUnscopedPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalClassEmpty) {
    ExpectEnum("NaturalClassEmpty", 4, ENUM_SCOPE_CLASS, {});
}

TEST(EnumTests, NaturalClassSingle) {
    ExpectEnum("NaturalClassSingle", 5, ENUM_SCOPE_CLASS, {{"NaturalClassSingleItem", "0"}});
}

TEST(EnumTests, NaturalClassPair) {
    ExpectEnum("NaturalClassPair", 6, ENUM_SCOPE_CLASS, {
        {"NaturalClassPairFirst", "0"},
        {"NaturalClassPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalStructEmpty) {
    ExpectEnum("NaturalStructEmpty", 7, ENUM_SCOPE_STRUCT, {});
}

TEST(EnumTests, NaturalStructSingle) {
    ExpectEnum("NaturalStructSingle", 8, ENUM_SCOPE_STRUCT, {{"NaturalStructSingleItem", "0"}});
}

TEST(EnumTests, NaturalStructPair) {
    ExpectEnum("NaturalStructPair", 9, ENUM_SCOPE_STRUCT, {
        {"NaturalStructPairFirst", "0"},
        {"NaturalStructPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalUnscopedInt) {
    ExpectEnum("NaturalUnscopedInt", 10, ENUM_SCOPE_UNSCOPED, {}, "int");
}

TEST(EnumTests, NaturalClassInt) {
    ExpectEnum("NaturalClassInt", 11, ENUM_SCOPE_CLASS, {}, "int");
}

TEST(EnumTests, NaturalStructInt) {
    ExpectEnum("NaturalStructInt", 12, ENUM_SCOPE_STRUCT, {}, "int");
}

TEST(EnumTests, AnonymousUnscoped) {
    const TLEnumDeclaration* declaration = nullptr;
    for (const auto& candidate : EnumDeclarations() | std::views::values) {
        if (candidate.enumerators_size() == 1
            && candidate.enumerators(0).identifier().name() == "AnonymousUnscopedItem") {
            declaration = &candidate;
            break;
        }
    }

    ASSERT_NE(declaration, nullptr) << "No parser output for the anonymous unscoped enum";
    UEMeta::Testing::ExpectDeclarationMetadata(
        declaration->metadata(),
        "",
        AnonymousEnumQualifiedName(),
        EnumSourcePath(),
        13,
        true);
    ASSERT_TRUE(declaration->has_details());
    UEMeta::Testing::ExpectEnumDetails(declaration->details(), ENUM_SCOPE_UNSCOPED);

    const auto& enumerator = declaration->enumerators(0);
    UEMeta::Testing::ExpectIdentifier(
        enumerator.identifier(),
        "AnonymousUnscopedItem",
        "UEMeta::Testing::Types::AnonymousUnscopedItem",
        EnumSourcePath());
    EXPECT_EQ(UEMeta::Testing::VersionedValue(enumerator.value()), "0");
}

TEST(EnumTests, AssignedUnscopedEmpty) {
    ExpectEnum("AssignedUnscopedEmpty", 14, ENUM_SCOPE_UNSCOPED, {});
}

TEST(EnumTests, AssignedUnscopedSingle) {
    ExpectEnum("AssignedUnscopedSingle", 15, ENUM_SCOPE_UNSCOPED, {{"AssignedUnscopedSingleItem", "17"}});
}

TEST(EnumTests, AssignedUnscopedPair) {
    ExpectEnum("AssignedUnscopedPair", 16, ENUM_SCOPE_UNSCOPED, {
        {"AssignedUnscopedPairFirst", "-11"},
        {"AssignedUnscopedPairSecond", "42"}
    });
}

TEST(EnumTests, AssignedClassEmpty) {
    ExpectEnum("AssignedClassEmpty", 17, ENUM_SCOPE_CLASS, {});
}

TEST(EnumTests, AssignedClassSingle) {
    ExpectEnum("AssignedClassSingle", 18, ENUM_SCOPE_CLASS, {{"AssignedClassSingleItem", "29"}});
}

TEST(EnumTests, AssignedClassPair) {
    ExpectEnum("AssignedClassPair", 19, ENUM_SCOPE_CLASS, {
        {"AssignedClassPairFirst", "-7"},
        {"AssignedClassPairSecond", "81"}
    });
}

TEST(EnumTests, AssignedStructEmpty) {
    ExpectEnum("AssignedStructEmpty", 20, ENUM_SCOPE_STRUCT, {});
}

TEST(EnumTests, AssignedStructSingle) {
    ExpectEnum("AssignedStructSingle", 21, ENUM_SCOPE_STRUCT, {{"AssignedStructSingleItem", "-33"}});
}

TEST(EnumTests, AssignedStructPair) {
    ExpectEnum("AssignedStructPair", 22, ENUM_SCOPE_STRUCT, {
        {"AssignedStructPairFirst", "14"},
        {"AssignedStructPairSecond", "99"}
    });
}

TEST(EnumTests, AssignedUnscopedInt) {
    ExpectEnum("AssignedUnscopedInt", 23, ENUM_SCOPE_UNSCOPED, {{"AssignedUnscopedIntItem", "101"}}, "int");
}

TEST(EnumTests, AssignedClassInt) {
    ExpectEnum("AssignedClassInt", 24, ENUM_SCOPE_CLASS, {{"AssignedClassIntItem", "-101"}}, "int");
}

TEST(EnumTests, AssignedStructInt) {
    ExpectEnum("AssignedStructInt", 25, ENUM_SCOPE_STRUCT, {{"AssignedStructIntItem", "303"}}, "int");
}
