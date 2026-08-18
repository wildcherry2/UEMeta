#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "parser.pb.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

    enum class TypeCase {
        Plain,
        Pointer,
        DoublePointer,
        Array,
        Reference
    };

    enum class StorageCase {
        Unspecified,
        Extern,
        ExternC,
        Static,
        ThreadLocal
    };

    enum class EvaluationCase {
        None,
        Constexpr,
        Consteval
    };

    struct ExpectedVariable {
        std::string name;
        std::string type;
        std::string underlying_type;
        std::string as_string;
        std::optional<std::string> default_value;
        VariableStorageClass storage_class;
        ConstantEvaluationKind evaluation_kind;
        std::uint32_t occurrence_index;
    };

    void PrintTo(const ExpectedVariable& variable, std::ostream* output) {
        *output << variable.name;
    }

    constexpr std::array TypeCases{
        TypeCase::Plain,
        TypeCase::Pointer,
        TypeCase::DoublePointer,
        TypeCase::Array,
        TypeCase::Reference
    };

    constexpr std::array StorageCases{
        StorageCase::Unspecified,
        StorageCase::Extern,
        StorageCase::ExternC,
        StorageCase::Static,
        StorageCase::ThreadLocal
    };

    constexpr std::array EvaluationCases{
        EvaluationCase::None,
        EvaluationCase::Constexpr,
        EvaluationCase::Consteval
    };

    constexpr std::array DefaultValueCases{false, true};

    fs::path GlobalVariableSourcePath() {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "GlobalVariableTypes.cpp";
    }

    std::string QualifiedName(const std::string_view name) {
        return "UEMeta::Testing::Types::" + std::string{name};
    }

    std::string_view TypeName(const TypeCase type) {
        switch (type) {
            case TypeCase::Plain: return "Plain";
            case TypeCase::Pointer: return "Pointer";
            case TypeCase::DoublePointer: return "DoublePointer";
            case TypeCase::Array: return "Array";
            case TypeCase::Reference: return "Reference";
        }
        return {};
    }

    std::string_view StorageName(const StorageCase storage) {
        switch (storage) {
            case StorageCase::Unspecified: return "Unspecified";
            case StorageCase::Extern: return "Extern";
            case StorageCase::ExternC: return "ExternC";
            case StorageCase::Static: return "Static";
            case StorageCase::ThreadLocal: return "ThreadLocal";
        }
        return {};
    }

    std::string_view EvaluationName(const EvaluationCase evaluation) {
        switch (evaluation) {
            case EvaluationCase::None: return "None";
            case EvaluationCase::Constexpr: return "Constexpr";
            case EvaluationCase::Consteval: return "Consteval";
        }
        return {};
    }

    bool IsLegal(
        const TypeCase type,
        const StorageCase storage,
        const EvaluationCase evaluation,
        const bool has_default_value) {
        if (evaluation == EvaluationCase::Consteval) {
            return false;
        }
        if (evaluation == EvaluationCase::Constexpr && !has_default_value) {
            return false;
        }
        if (type == TypeCase::Reference && !has_default_value) {
            return storage == StorageCase::Extern || storage == StorageCase::ExternC;
        }
        return true;
    }

    VariableStorageClass ExpectedStorageClass(const StorageCase storage) {
        switch (storage) {
            case StorageCase::Unspecified: return VAR_STORAGE_CLASS_UNSPECIFIED;
            case StorageCase::Extern: return VAR_STORAGE_CLASS_EXTERN;
            case StorageCase::ExternC: return VAR_STORAGE_CLASS_EXTERN_C;
            case StorageCase::Static: return VAR_STORAGE_CLASS_STATIC;
            case StorageCase::ThreadLocal: return VAR_STORAGE_CLASS_THREAD_LOCAL;
        }
        return VAR_STORAGE_CLASS_UNSPECIFIED;
    }

    ConstantEvaluationKind ExpectedEvaluationKind(const EvaluationCase evaluation) {
        return evaluation == EvaluationCase::Constexpr
            ? CONSTANT_EVALUATION_CONSTEXPR
            : CONSTANT_EVALUATION_NONE;
    }

    std::string ExpectedType(const TypeCase type, const EvaluationCase evaluation) {
        const auto is_constexpr = evaluation == EvaluationCase::Constexpr;
        switch (type) {
            case TypeCase::Plain: return is_constexpr ? "const int" : "int";
            case TypeCase::Pointer: return is_constexpr ? "int *const" : "int *";
            case TypeCase::DoublePointer: return is_constexpr ? "int **const" : "int **";
            case TypeCase::Array: return is_constexpr ? "const int[2]" : "int[2]";
            case TypeCase::Reference: return "int &";
        }
        return {};
    }

    std::string ExpectedInitializer(const TypeCase type) {
        switch (type) {
            case TypeCase::Plain: return "17";
            case TypeCase::Pointer:
            case TypeCase::DoublePointer: return "nullptr";
            case TypeCase::Array: return "{17}";
            case TypeCase::Reference: return "PlainExternNoneWithoutDefault";
        }
        return {};
    }

    std::string ExpectedDeclarator(const TypeCase type, const std::string_view name) {
        switch (type) {
            case TypeCase::Plain: return "int " + std::string{name};
            case TypeCase::Pointer: return "int *" + std::string{name};
            case TypeCase::DoublePointer: return "int **" + std::string{name};
            case TypeCase::Array: return "int " + std::string{name} + "[2]";
            case TypeCase::Reference: return "int &" + std::string{name};
        }
        return {};
    }

    std::string ExpectedStoragePrefix(const StorageCase storage) {
        switch (storage) {
            case StorageCase::Unspecified: return {};
            case StorageCase::Extern:
            case StorageCase::ExternC: return "extern ";
            case StorageCase::Static: return "static ";
            case StorageCase::ThreadLocal: return "thread_local ";
        }
        return {};
    }

    std::vector<ExpectedVariable> BuildExpectedVariables() {
        std::vector<ExpectedVariable> result;

        for (const auto type : TypeCases) {
            for (const auto storage : StorageCases) {
                for (const auto evaluation : EvaluationCases) {
                    for (const auto has_default_value : DefaultValueCases) {
                        if (!IsLegal(type, storage, evaluation, has_default_value)) {
                            continue;
                        }

                        auto name = std::string{TypeName(type)}
                            + std::string{StorageName(storage)}
                            + std::string{EvaluationName(evaluation)}
                            + (has_default_value ? "WithDefault" : "WithoutDefault");
                        auto as_string = ExpectedStoragePrefix(storage)
                            + (evaluation == EvaluationCase::Constexpr ? "constexpr " : "")
                            + ExpectedDeclarator(type, name);
                        std::optional<std::string> default_value;
                        if (has_default_value) {
                            default_value = ExpectedInitializer(type);
                            as_string += " = " + *default_value;
                        }

                        result.push_back({
                            std::move(name),
                            ExpectedType(type, evaluation),
                            "int",
                            std::move(as_string),
                            std::move(default_value),
                            ExpectedStorageClass(storage),
                            ExpectedEvaluationKind(evaluation),
                            static_cast<std::uint32_t>(result.size())
                        });
                    }
                }
            }
        }

        return result;
    }

    const std::vector<ExpectedVariable>& ExpectedVariables() {
        static const auto variables = BuildExpectedVariables();
        return variables;
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
}

class GlobalVariableTests : public testing::TestWithParam<ExpectedVariable> {};

TEST_P(GlobalVariableTests, SerializesLegalCombination) {
    const auto& expected = GetParam();
    SCOPED_TRACE(expected.name);

    const auto& declarations = GlobalVariableDeclarations();
    const auto found = declarations.find(expected.name);
    ASSERT_NE(found, declarations.end()) << "No parser output for global variable " << expected.name;

    const auto& declaration = found->second;
    ASSERT_TRUE(declaration.has_metadata());
    UEMeta::Testing::ExpectDeclarationMetadata(
        declaration.metadata(),
        expected.name,
        QualifiedName(expected.name),
        GlobalVariableSourcePath(),
        expected.occurrence_index,
        false);

    EXPECT_FALSE(declaration.has_template_details());
    EXPECT_EQ(declaration.type(), expected.type);
    EXPECT_EQ(declaration.underlying_type(), expected.underlying_type);
    EXPECT_EQ(declaration.as_string(), expected.as_string);

    EXPECT_TRUE(declaration.has_storage_class());
    EXPECT_EQ(declaration.storage_class(), expected.storage_class);
    EXPECT_TRUE(declaration.has_constant_evaluation_kind());
    EXPECT_EQ(declaration.constant_evaluation_kind(), expected.evaluation_kind);

    EXPECT_EQ(declaration.has_default_value(), expected.default_value.has_value());
    if (expected.default_value) {
        EXPECT_EQ(declaration.default_value(), *expected.default_value);
    }

    const auto expected_content_hash = std::hash<std::string>{}(expected.as_string);
    EXPECT_EQ(declaration.metadata().content_hash(), expected_content_hash);
    EXPECT_EQ(declaration.content_hash(), expected_content_hash);
}

INSTANTIATE_TEST_SUITE_P(
    ExhaustiveLegalCombinations,
    GlobalVariableTests,
    testing::ValuesIn(ExpectedVariables()),
    [](const testing::TestParamInfo<ExpectedVariable>& info) {
        return info.param.name;
    });

TEST(GlobalVariableCoverageTests, ContainsEveryLegalCombinationAndNoExtras) {
    EXPECT_EQ(TypeCases.size() * StorageCases.size() * EvaluationCases.size() * DefaultValueCases.size(), 150);
    EXPECT_EQ(ExpectedVariables().size(), 72);
    EXPECT_EQ(GlobalVariableDeclarations().size(), ExpectedVariables().size());
}
