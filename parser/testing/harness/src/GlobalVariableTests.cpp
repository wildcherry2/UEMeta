#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "parser.pb.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::CONSTANT_EVALUATION_CONSTEXPR;
    using ParseResult::CONSTANT_EVALUATION_NONE;
    using ParseResult::ConstantEvaluationKind;
    using ParseResult::TLGlobalVariableDeclaration;
    using ParseResult::VAR_STORAGE_CLASS_EXTERN;
    using ParseResult::VAR_STORAGE_CLASS_EXTERN_C;
    using ParseResult::VAR_STORAGE_CLASS_STATIC;
    using ParseResult::VAR_STORAGE_CLASS_THREAD_LOCAL;
    using ParseResult::VAR_STORAGE_CLASS_UNSPECIFIED;
    using ParseResult::VariableStorageClass;

    fs::path GlobalVariableSourcePath() {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "GlobalVariableTypes.cpp";
    }

    std::string QualifiedName(const std::string_view name) {
        return "UEMeta::Testing::Types::" + std::string{name};
    }

    const std::unordered_map<std::string, TLGlobalVariableDeclaration>& GlobalVariableDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLGlobalVariableDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".varbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLGlobalVariableDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse global-variable output " << entry.path();
                    continue;
                }

                if (declaration.metadata().identifier().file_path()
                    != GlobalVariableSourcePath().string()) {
                    continue;
                }

                result.insert_or_assign(
                    declaration.metadata().identifier().name(),
                    std::move(declaration));
            }

            return result;
        }();

        return declarations;
    }

    void ExpectGlobalVariable(
        const std::string_view expected_name,
        const std::uint32_t expected_occurrence_index,
        const std::string_view expected_type,
        const std::string_view expected_as_string,
        const VariableStorageClass expected_storage_class,
        const ConstantEvaluationKind expected_evaluation_kind,
        const std::optional<std::string_view> expected_default_value) {
        SCOPED_TRACE(expected_name);

        const auto& declarations = GlobalVariableDeclarations();
        const auto found = declarations.find(std::string{expected_name});
        ASSERT_NE(found, declarations.end())
            << "No parser output for global variable " << expected_name;

        const auto& declaration = found->second;
        ASSERT_TRUE(declaration.has_metadata());
        UEMeta::Testing::ExpectDeclarationMetadata(
            declaration.metadata(),
            expected_name,
            QualifiedName(expected_name),
            GlobalVariableSourcePath(),
            expected_occurrence_index,
            false);

        EXPECT_FALSE(declaration.has_template_details());
        EXPECT_EQ(declaration.type(), expected_type);
        EXPECT_EQ(declaration.underlying_type(), "int");
        EXPECT_EQ(declaration.as_string(), expected_as_string);

        ASSERT_TRUE(declaration.has_storage_class());
        EXPECT_EQ(declaration.storage_class(), expected_storage_class);
        ASSERT_TRUE(declaration.has_constant_evaluation_kind());
        EXPECT_EQ(declaration.constant_evaluation_kind(), expected_evaluation_kind);

        EXPECT_EQ(declaration.has_default_value(), expected_default_value.has_value());
        if (expected_default_value) {
            EXPECT_EQ(declaration.default_value(), *expected_default_value);
        }

        const auto expected_content_hash =
            std::hash<std::string_view>{}(expected_as_string);
        EXPECT_EQ(declaration.metadata().content_hash(), expected_content_hash);
        EXPECT_EQ(declaration.content_hash(), expected_content_hash);
    }
}

TEST(GlobalVariableTests, PlainUnspecifiedNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PlainUnspecifiedNoneWithoutDefault",
        0,
        "int",
        R"(int PlainUnspecifiedNoneWithoutDefault)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PlainUnspecifiedNoneWithDefault) {
    ExpectGlobalVariable(
        "PlainUnspecifiedNoneWithDefault",
        1,
        "int",
        R"(int PlainUnspecifiedNoneWithDefault = 17)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        R"(17)");
}

TEST(GlobalVariableTests, PlainUnspecifiedConstexprWithDefault) {
    ExpectGlobalVariable(
        "PlainUnspecifiedConstexprWithDefault",
        2,
        "const int",
        R"(constexpr int PlainUnspecifiedConstexprWithDefault = 17)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(17)");
}

TEST(GlobalVariableTests, PlainExternNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PlainExternNoneWithoutDefault",
        3,
        "int",
        R"(extern int PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PlainExternNoneWithDefault) {
    ExpectGlobalVariable(
        "PlainExternNoneWithDefault",
        4,
        "int",
        R"(extern int PlainExternNoneWithDefault = 17)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        R"(17)");
}

TEST(GlobalVariableTests, PlainExternConstexprWithDefault) {
    ExpectGlobalVariable(
        "PlainExternConstexprWithDefault",
        5,
        "const int",
        R"(extern constexpr int PlainExternConstexprWithDefault = 17)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(17)");
}

TEST(GlobalVariableTests, PlainExternCNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PlainExternCNoneWithoutDefault",
        6,
        "int",
        R"(extern int PlainExternCNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PlainExternCNoneWithDefault) {
    ExpectGlobalVariable(
        "PlainExternCNoneWithDefault",
        7,
        "int",
        R"(extern int PlainExternCNoneWithDefault = 17)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        R"(17)");
}

TEST(GlobalVariableTests, PlainExternCConstexprWithDefault) {
    ExpectGlobalVariable(
        "PlainExternCConstexprWithDefault",
        8,
        "const int",
        R"(extern constexpr int PlainExternCConstexprWithDefault = 17)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(17)");
}

TEST(GlobalVariableTests, PlainStaticNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PlainStaticNoneWithoutDefault",
        9,
        "int",
        R"(static int PlainStaticNoneWithoutDefault)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PlainStaticNoneWithDefault) {
    ExpectGlobalVariable(
        "PlainStaticNoneWithDefault",
        10,
        "int",
        R"(static int PlainStaticNoneWithDefault = 17)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        R"(17)");
}

TEST(GlobalVariableTests, PlainStaticConstexprWithDefault) {
    ExpectGlobalVariable(
        "PlainStaticConstexprWithDefault",
        11,
        "const int",
        R"(static constexpr int PlainStaticConstexprWithDefault = 17)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(17)");
}

TEST(GlobalVariableTests, PlainThreadLocalNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PlainThreadLocalNoneWithoutDefault",
        12,
        "int",
        R"(thread_local int PlainThreadLocalNoneWithoutDefault)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PlainThreadLocalNoneWithDefault) {
    ExpectGlobalVariable(
        "PlainThreadLocalNoneWithDefault",
        13,
        "int",
        R"(thread_local int PlainThreadLocalNoneWithDefault = 17)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        R"(17)");
}

TEST(GlobalVariableTests, PlainThreadLocalConstexprWithDefault) {
    ExpectGlobalVariable(
        "PlainThreadLocalConstexprWithDefault",
        14,
        "const int",
        R"(thread_local constexpr int PlainThreadLocalConstexprWithDefault = 17)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(17)");
}

TEST(GlobalVariableTests, PointerUnspecifiedNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PointerUnspecifiedNoneWithoutDefault",
        15,
        "int *",
        R"(int *PointerUnspecifiedNoneWithoutDefault)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PointerUnspecifiedNoneWithDefault) {
    ExpectGlobalVariable(
        "PointerUnspecifiedNoneWithDefault",
        16,
        "int *",
        R"(int *PointerUnspecifiedNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerUnspecifiedConstexprWithDefault) {
    ExpectGlobalVariable(
        "PointerUnspecifiedConstexprWithDefault",
        17,
        "int *const",
        R"(constexpr int *PointerUnspecifiedConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerExternNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PointerExternNoneWithoutDefault",
        18,
        "int *",
        R"(extern int *PointerExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PointerExternNoneWithDefault) {
    ExpectGlobalVariable(
        "PointerExternNoneWithDefault",
        19,
        "int *",
        R"(extern int *PointerExternNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerExternConstexprWithDefault) {
    ExpectGlobalVariable(
        "PointerExternConstexprWithDefault",
        20,
        "int *const",
        R"(extern constexpr int *PointerExternConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerExternCNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PointerExternCNoneWithoutDefault",
        21,
        "int *",
        R"(extern int *PointerExternCNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PointerExternCNoneWithDefault) {
    ExpectGlobalVariable(
        "PointerExternCNoneWithDefault",
        22,
        "int *",
        R"(extern int *PointerExternCNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerExternCConstexprWithDefault) {
    ExpectGlobalVariable(
        "PointerExternCConstexprWithDefault",
        23,
        "int *const",
        R"(extern constexpr int *PointerExternCConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerStaticNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PointerStaticNoneWithoutDefault",
        24,
        "int *",
        R"(static int *PointerStaticNoneWithoutDefault)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PointerStaticNoneWithDefault) {
    ExpectGlobalVariable(
        "PointerStaticNoneWithDefault",
        25,
        "int *",
        R"(static int *PointerStaticNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerStaticConstexprWithDefault) {
    ExpectGlobalVariable(
        "PointerStaticConstexprWithDefault",
        26,
        "int *const",
        R"(static constexpr int *PointerStaticConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerThreadLocalNoneWithoutDefault) {
    ExpectGlobalVariable(
        "PointerThreadLocalNoneWithoutDefault",
        27,
        "int *",
        R"(thread_local int *PointerThreadLocalNoneWithoutDefault)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, PointerThreadLocalNoneWithDefault) {
    ExpectGlobalVariable(
        "PointerThreadLocalNoneWithDefault",
        28,
        "int *",
        R"(thread_local int *PointerThreadLocalNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, PointerThreadLocalConstexprWithDefault) {
    ExpectGlobalVariable(
        "PointerThreadLocalConstexprWithDefault",
        29,
        "int *const",
        R"(thread_local constexpr int *PointerThreadLocalConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerUnspecifiedNoneWithoutDefault) {
    ExpectGlobalVariable(
        "DoublePointerUnspecifiedNoneWithoutDefault",
        30,
        "int **",
        R"(int **DoublePointerUnspecifiedNoneWithoutDefault)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, DoublePointerUnspecifiedNoneWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerUnspecifiedNoneWithDefault",
        31,
        "int **",
        R"(int **DoublePointerUnspecifiedNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerUnspecifiedConstexprWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerUnspecifiedConstexprWithDefault",
        32,
        "int **const",
        R"(constexpr int **DoublePointerUnspecifiedConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerExternNoneWithoutDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternNoneWithoutDefault",
        33,
        "int **",
        R"(extern int **DoublePointerExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, DoublePointerExternNoneWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternNoneWithDefault",
        34,
        "int **",
        R"(extern int **DoublePointerExternNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerExternConstexprWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternConstexprWithDefault",
        35,
        "int **const",
        R"(extern constexpr int **DoublePointerExternConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerExternCNoneWithoutDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternCNoneWithoutDefault",
        36,
        "int **",
        R"(extern int **DoublePointerExternCNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, DoublePointerExternCNoneWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternCNoneWithDefault",
        37,
        "int **",
        R"(extern int **DoublePointerExternCNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerExternCConstexprWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerExternCConstexprWithDefault",
        38,
        "int **const",
        R"(extern constexpr int **DoublePointerExternCConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerStaticNoneWithoutDefault) {
    ExpectGlobalVariable(
        "DoublePointerStaticNoneWithoutDefault",
        39,
        "int **",
        R"(static int **DoublePointerStaticNoneWithoutDefault)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, DoublePointerStaticNoneWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerStaticNoneWithDefault",
        40,
        "int **",
        R"(static int **DoublePointerStaticNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerStaticConstexprWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerStaticConstexprWithDefault",
        41,
        "int **const",
        R"(static constexpr int **DoublePointerStaticConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerThreadLocalNoneWithoutDefault) {
    ExpectGlobalVariable(
        "DoublePointerThreadLocalNoneWithoutDefault",
        42,
        "int **",
        R"(thread_local int **DoublePointerThreadLocalNoneWithoutDefault)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, DoublePointerThreadLocalNoneWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerThreadLocalNoneWithDefault",
        43,
        "int **",
        R"(thread_local int **DoublePointerThreadLocalNoneWithDefault = nullptr)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        R"(nullptr)");
}

TEST(GlobalVariableTests, DoublePointerThreadLocalConstexprWithDefault) {
    ExpectGlobalVariable(
        "DoublePointerThreadLocalConstexprWithDefault",
        44,
        "int **const",
        R"(thread_local constexpr int **DoublePointerThreadLocalConstexprWithDefault = nullptr)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(nullptr)");
}

TEST(GlobalVariableTests, ArrayUnspecifiedNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ArrayUnspecifiedNoneWithoutDefault",
        45,
        "int[2]",
        R"(int ArrayUnspecifiedNoneWithoutDefault[2])",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ArrayUnspecifiedNoneWithDefault) {
    ExpectGlobalVariable(
        "ArrayUnspecifiedNoneWithDefault",
        46,
        "int[2]",
        R"(int ArrayUnspecifiedNoneWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayUnspecifiedConstexprWithDefault) {
    ExpectGlobalVariable(
        "ArrayUnspecifiedConstexprWithDefault",
        47,
        "const int[2]",
        R"(constexpr int ArrayUnspecifiedConstexprWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayExternNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ArrayExternNoneWithoutDefault",
        48,
        "int[2]",
        R"(extern int ArrayExternNoneWithoutDefault[2])",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ArrayExternNoneWithDefault) {
    ExpectGlobalVariable(
        "ArrayExternNoneWithDefault",
        49,
        "int[2]",
        R"(extern int ArrayExternNoneWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayExternConstexprWithDefault) {
    ExpectGlobalVariable(
        "ArrayExternConstexprWithDefault",
        50,
        "const int[2]",
        R"(extern constexpr int ArrayExternConstexprWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayExternCNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ArrayExternCNoneWithoutDefault",
        51,
        "int[2]",
        R"(extern int ArrayExternCNoneWithoutDefault[2])",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ArrayExternCNoneWithDefault) {
    ExpectGlobalVariable(
        "ArrayExternCNoneWithDefault",
        52,
        "int[2]",
        R"(extern int ArrayExternCNoneWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayExternCConstexprWithDefault) {
    ExpectGlobalVariable(
        "ArrayExternCConstexprWithDefault",
        53,
        "const int[2]",
        R"(extern constexpr int ArrayExternCConstexprWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayStaticNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ArrayStaticNoneWithoutDefault",
        54,
        "int[2]",
        R"(static int ArrayStaticNoneWithoutDefault[2])",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ArrayStaticNoneWithDefault) {
    ExpectGlobalVariable(
        "ArrayStaticNoneWithDefault",
        55,
        "int[2]",
        R"(static int ArrayStaticNoneWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayStaticConstexprWithDefault) {
    ExpectGlobalVariable(
        "ArrayStaticConstexprWithDefault",
        56,
        "const int[2]",
        R"(static constexpr int ArrayStaticConstexprWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayThreadLocalNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ArrayThreadLocalNoneWithoutDefault",
        57,
        "int[2]",
        R"(thread_local int ArrayThreadLocalNoneWithoutDefault[2])",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ArrayThreadLocalNoneWithDefault) {
    ExpectGlobalVariable(
        "ArrayThreadLocalNoneWithDefault",
        58,
        "int[2]",
        R"(thread_local int ArrayThreadLocalNoneWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        R"({17})");
}

TEST(GlobalVariableTests, ArrayThreadLocalConstexprWithDefault) {
    ExpectGlobalVariable(
        "ArrayThreadLocalConstexprWithDefault",
        59,
        "const int[2]",
        R"(thread_local constexpr int ArrayThreadLocalConstexprWithDefault[2] = {17})",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"({17})");
}

TEST(GlobalVariableTests, ReferenceUnspecifiedNoneWithDefault) {
    ExpectGlobalVariable(
        "ReferenceUnspecifiedNoneWithDefault",
        60,
        "int &",
        R"(int &ReferenceUnspecifiedNoneWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_NONE,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceUnspecifiedConstexprWithDefault) {
    ExpectGlobalVariable(
        "ReferenceUnspecifiedConstexprWithDefault",
        61,
        "int &",
        R"(constexpr int &ReferenceUnspecifiedConstexprWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_UNSPECIFIED,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceExternNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ReferenceExternNoneWithoutDefault",
        62,
        "int &",
        R"(extern int &ReferenceExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ReferenceExternNoneWithDefault) {
    ExpectGlobalVariable(
        "ReferenceExternNoneWithDefault",
        63,
        "int &",
        R"(extern int &ReferenceExternNoneWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_NONE,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceExternConstexprWithDefault) {
    ExpectGlobalVariable(
        "ReferenceExternConstexprWithDefault",
        64,
        "int &",
        R"(extern constexpr int &ReferenceExternConstexprWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceExternCNoneWithoutDefault) {
    ExpectGlobalVariable(
        "ReferenceExternCNoneWithoutDefault",
        65,
        "int &",
        R"(extern int &ReferenceExternCNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        std::nullopt);
}

TEST(GlobalVariableTests, ReferenceExternCNoneWithDefault) {
    ExpectGlobalVariable(
        "ReferenceExternCNoneWithDefault",
        66,
        "int &",
        R"(extern int &ReferenceExternCNoneWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_NONE,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceExternCConstexprWithDefault) {
    ExpectGlobalVariable(
        "ReferenceExternCConstexprWithDefault",
        67,
        "int &",
        R"(extern constexpr int &ReferenceExternCConstexprWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_EXTERN_C,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceStaticNoneWithDefault) {
    ExpectGlobalVariable(
        "ReferenceStaticNoneWithDefault",
        68,
        "int &",
        R"(static int &ReferenceStaticNoneWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_NONE,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceStaticConstexprWithDefault) {
    ExpectGlobalVariable(
        "ReferenceStaticConstexprWithDefault",
        69,
        "int &",
        R"(static constexpr int &ReferenceStaticConstexprWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_STATIC,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceThreadLocalNoneWithDefault) {
    ExpectGlobalVariable(
        "ReferenceThreadLocalNoneWithDefault",
        70,
        "int &",
        R"(thread_local int &ReferenceThreadLocalNoneWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_NONE,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableTests, ReferenceThreadLocalConstexprWithDefault) {
    ExpectGlobalVariable(
        "ReferenceThreadLocalConstexprWithDefault",
        71,
        "int &",
        R"(thread_local constexpr int &ReferenceThreadLocalConstexprWithDefault = PlainExternNoneWithoutDefault)",
        VAR_STORAGE_CLASS_THREAD_LOCAL,
        CONSTANT_EVALUATION_CONSTEXPR,
        R"(PlainExternNoneWithoutDefault)");
}

TEST(GlobalVariableCoverageTests, AccountsForEveryTargetDeclaration) {
    EXPECT_EQ(GlobalVariableDeclarations().size(), 72);
}
