#include <gtest/gtest.h>

#include "parser.pb.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
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
        const EnumScope scope,
        const std::initializer_list<ExpectedEnumerator> expected_enumerators,
        const std::string_view underlying_type = {}) {
        SCOPED_TRACE(enum_name);

        const auto& declarations = EnumDeclarations();
        const auto found = declarations.find(std::string{enum_name});
        ASSERT_NE(found, declarations.end()) << "No parser output for enum " << enum_name;

        const auto& declaration = found->second;
        ASSERT_TRUE(declaration.has_details());
        EXPECT_TRUE(declaration.details().has_scope());
        EXPECT_EQ(declaration.details().scope(), scope);

        if (!underlying_type.empty()) {
            EXPECT_TRUE(declaration.details().has_underlying_type());
            EXPECT_EQ(declaration.details().underlying_type(), underlying_type);
        }

        ASSERT_EQ(declaration.enumerators_size(), expected_enumerators.size());
        int index = 0;
        for (const auto& expected : expected_enumerators) {
            const auto& enumerator = declaration.enumerators(index++);
            ASSERT_TRUE(enumerator.has_identifier());
            EXPECT_EQ(enumerator.identifier().name(), expected.identifier);
            EXPECT_EQ(enumerator.value(), expected.value);
        }
    }
}

TEST(EnumTests, NaturalUnscopedEmpty) {
    ExpectEnum("NaturalUnscopedEmpty", ENUM_SCOPE_UNSCOPED, {});
}

TEST(EnumTests, NaturalUnscopedSingle) {
    ExpectEnum("NaturalUnscopedSingle", ENUM_SCOPE_UNSCOPED, {{"NaturalUnscopedSingleItem", "0"}});
}

TEST(EnumTests, NaturalUnscopedPair) {
    ExpectEnum("NaturalUnscopedPair", ENUM_SCOPE_UNSCOPED, {
        {"NaturalUnscopedPairFirst", "0"},
        {"NaturalUnscopedPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalClassEmpty) {
    ExpectEnum("NaturalClassEmpty", ENUM_SCOPE_CLASS, {});
}

TEST(EnumTests, NaturalClassSingle) {
    ExpectEnum("NaturalClassSingle", ENUM_SCOPE_CLASS, {{"NaturalClassSingleItem", "0"}});
}

TEST(EnumTests, NaturalClassPair) {
    ExpectEnum("NaturalClassPair", ENUM_SCOPE_CLASS, {
        {"NaturalClassPairFirst", "0"},
        {"NaturalClassPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalStructEmpty) {
    ExpectEnum("NaturalStructEmpty", ENUM_SCOPE_STRUCT, {});
}

TEST(EnumTests, NaturalStructSingle) {
    ExpectEnum("NaturalStructSingle", ENUM_SCOPE_STRUCT, {{"NaturalStructSingleItem", "0"}});
}

TEST(EnumTests, NaturalStructPair) {
    ExpectEnum("NaturalStructPair", ENUM_SCOPE_STRUCT, {
        {"NaturalStructPairFirst", "0"},
        {"NaturalStructPairSecond", "1"}
    });
}

TEST(EnumTests, NaturalUnscopedInt) {
    ExpectEnum("NaturalUnscopedInt", ENUM_SCOPE_UNSCOPED, {}, "int");
}

TEST(EnumTests, NaturalClassInt) {
    ExpectEnum("NaturalClassInt", ENUM_SCOPE_CLASS, {}, "int");
}

TEST(EnumTests, NaturalStructInt) {
    ExpectEnum("NaturalStructInt", ENUM_SCOPE_STRUCT, {}, "int");
}

TEST(EnumTests, AssignedUnscopedEmpty) {
    ExpectEnum("AssignedUnscopedEmpty", ENUM_SCOPE_UNSCOPED, {});
}

TEST(EnumTests, AssignedUnscopedSingle) {
    ExpectEnum("AssignedUnscopedSingle", ENUM_SCOPE_UNSCOPED, {{"AssignedUnscopedSingleItem", "17"}});
}

TEST(EnumTests, AssignedUnscopedPair) {
    ExpectEnum("AssignedUnscopedPair", ENUM_SCOPE_UNSCOPED, {
        {"AssignedUnscopedPairFirst", "-11"},
        {"AssignedUnscopedPairSecond", "42"}
    });
}

TEST(EnumTests, AssignedClassEmpty) {
    ExpectEnum("AssignedClassEmpty", ENUM_SCOPE_CLASS, {});
}

TEST(EnumTests, AssignedClassSingle) {
    ExpectEnum("AssignedClassSingle", ENUM_SCOPE_CLASS, {{"AssignedClassSingleItem", "29"}});
}

TEST(EnumTests, AssignedClassPair) {
    ExpectEnum("AssignedClassPair", ENUM_SCOPE_CLASS, {
        {"AssignedClassPairFirst", "-7"},
        {"AssignedClassPairSecond", "81"}
    });
}

TEST(EnumTests, AssignedStructEmpty) {
    ExpectEnum("AssignedStructEmpty", ENUM_SCOPE_STRUCT, {});
}

TEST(EnumTests, AssignedStructSingle) {
    ExpectEnum("AssignedStructSingle", ENUM_SCOPE_STRUCT, {{"AssignedStructSingleItem", "-33"}});
}

TEST(EnumTests, AssignedStructPair) {
    ExpectEnum("AssignedStructPair", ENUM_SCOPE_STRUCT, {
        {"AssignedStructPairFirst", "14"},
        {"AssignedStructPairSecond", "99"}
    });
}

TEST(EnumTests, AssignedUnscopedInt) {
    ExpectEnum("AssignedUnscopedInt", ENUM_SCOPE_UNSCOPED, {{"AssignedUnscopedIntItem", "101"}}, "int");
}

TEST(EnumTests, AssignedClassInt) {
    ExpectEnum("AssignedClassInt", ENUM_SCOPE_CLASS, {{"AssignedClassIntItem", "-101"}}, "int");
}

TEST(EnumTests, AssignedStructInt) {
    ExpectEnum("AssignedStructInt", ENUM_SCOPE_STRUCT, {{"AssignedStructIntItem", "303"}}, "int");
}
