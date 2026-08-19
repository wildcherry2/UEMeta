#include <gtest/gtest.h>

#include "DeclarationMetadataTestHelpers.hpp"
#include "FunctionCommonTestHelpers.hpp"
#include "parser.pb.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::CONSTANT_EVALUATION_CONSTEVAL;
    using ParseResult::CONSTANT_EVALUATION_CONSTEXPR;
    using ParseResult::CONSTANT_EVALUATION_NONE;
    using ParseResult::ConstantEvaluationKind;
    using ParseResult::FUN_VAR_STORAGE_CLASS_EXTERN;
    using ParseResult::FUN_VAR_STORAGE_CLASS_EXTERN_C;
    using ParseResult::FUN_VAR_STORAGE_CLASS_STATIC;
    using ParseResult::FUN_VAR_STORAGE_CLASS_UNSPECIFIED;
    using ParseResult::FUNCTION_KIND_FREE;
    using ParseResult::FunctionStorageClass;
    using ParseResult::TEMPLATE_PARAMETER_KIND_TYPENAME;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION;
    using ParseResult::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION;
    using ParseResult::TEMPLATE_SPECIALIZATION_IMPLICIT;
    using ParseResult::TEMPLATE_SPECIALIZATION_NONE;
    using ParseResult::TemplateSpecializationKind;
    using ParseResult::TLFreeFunctionDeclaration;
    using UEMeta::Testing::ExpectedFunctionParameter;
    using UEMeta::Testing::ExpectedFunctionTemplateDetails;

    enum class SourceReturnType { Auto, Void, Int };

    fs::path FreeFunctionSourcePath() { return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / "FreeFunctionTypes.hpp"; }

    std::string QualifiedName(const std::string_view name) { return "UEMeta::Testing::Types::" + std::string{name}; }

    std::string FreeFunctionKey(const std::string_view name,
                                const std::optional<TemplateSpecializationKind> specialization_kind) {
        if (!specialization_kind) {
            return std::string{name} + ":NoTemplate";
        }
        return std::string{name} + ":" + std::to_string(static_cast<int>(*specialization_kind));
    }

    std::string FreeFunctionKey(const TLFreeFunctionDeclaration& declaration) {
        const auto& common = declaration.common();
        if (!common.has_template_details()) {
            return FreeFunctionKey(common.identifier().name(), std::nullopt);
        }
        if (!common.template_details().has_specialization_kind()) {
            ADD_FAILURE() << "Templated function has no specialization kind: " << common.identifier().qualified_name();
            return FreeFunctionKey(common.identifier().name(), std::nullopt);
        }
        return FreeFunctionKey(common.identifier().name(), common.template_details().specialization_kind());
    }

    const std::unordered_map<std::string, TLFreeFunctionDeclaration>& FreeFunctionDeclarations() {
        static const auto declarations = [] {
            std::unordered_map<std::string, TLFreeFunctionDeclaration> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".functionbin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLFreeFunctionDeclaration declaration;
                if (!input || !declaration.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse free-function output " << entry.path();
                    continue;
                }

                if (!declaration.has_metadata() ||
                    declaration.metadata().identifier().file_path() != FreeFunctionSourcePath().string()) {
                    continue;
                }
                if (!declaration.has_common()) {
                    ADD_FAILURE() << "Free-function declaration has no FunctionCommon: " << entry.path();
                    continue;
                }

                const auto key = FreeFunctionKey(declaration);
                const auto [_, inserted] = result.emplace(key, std::move(declaration));
                if (!inserted) {
                    ADD_FAILURE() << "Duplicate free-function declaration key " << key;
                }
            }

            return result;
        }();

        return declarations;
    }

    std::string TemplateArguments(const std::size_t template_parameter_count,
                                  const std::optional<TemplateSpecializationKind> specialization_kind) {
        if (!specialization_kind || *specialization_kind == TEMPLATE_SPECIALIZATION_NONE) {
            return {};
        }

        const bool has_two_parameters = template_parameter_count == 2;
        switch (*specialization_kind) {
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

    std::string_view EffectiveReturnType(const SourceReturnType source_return_type, const bool has_inline_definition,
                                         const std::optional<TemplateSpecializationKind> specialization_kind) {
        switch (source_return_type) {
        case SourceReturnType::Auto:
            if (specialization_kind &&
                (*specialization_kind == TEMPLATE_SPECIALIZATION_NONE ||
                 *specialization_kind == TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION)) {
                return "auto";
            }
            return has_inline_definition ? "int" : "auto";
        case SourceReturnType::Void:
            return "void";
        case SourceReturnType::Int:
            return "int";
        }
        return {};
    }

    std::string_view InlineDefinition(const SourceReturnType return_type) {
        return return_type == SourceReturnType::Void ? std::string_view{"{\n}\n"}
                                                     : std::string_view{"{\n    return 0;\n}\n"};
    }

    std::string FunctionParametersAsString(const std::size_t parameter_count) {
        switch (parameter_count) {
        case 0:
            return "()";
        case 1:
            return "(int First)";
        case 2:
            return "(int First, long Second)";
        default:
            return "(invalid)";
        }
    }

    std::string ExpectedAsString(const std::string_view name, const FunctionStorageClass storage_class,
                                 const ConstantEvaluationKind evaluation_kind, const bool has_inline_definition,
                                 const std::size_t function_parameter_count, const SourceReturnType source_return_type,
                                 const std::size_t template_parameter_count,
                                 const std::optional<TemplateSpecializationKind> specialization_kind) {
        std::string result;
        if (specialization_kind && *specialization_kind != TEMPLATE_SPECIALIZATION_NONE) {
            result = "template<> ";
        }

        if (storage_class == FUN_VAR_STORAGE_CLASS_EXTERN || storage_class == FUN_VAR_STORAGE_CLASS_EXTERN_C) {
            result += "extern ";
        } else if (storage_class == FUN_VAR_STORAGE_CLASS_STATIC) {
            result += "static ";
        }

        if (evaluation_kind == CONSTANT_EVALUATION_CONSTEXPR) {
            result += "constexpr ";
        } else if (evaluation_kind == CONSTANT_EVALUATION_CONSTEVAL) {
            result += "consteval ";
        }

        result += EffectiveReturnType(source_return_type, has_inline_definition, specialization_kind);
        result += ' ';
        result += name;

        const auto arguments = TemplateArguments(template_parameter_count, specialization_kind);
        if (!arguments.empty()) {
            result += '<';
            result += arguments;
            result += '>';
        }

        result += FunctionParametersAsString(function_parameter_count);
        return result;
    }

    void ExpectFunctionCommonWithParameters(
        const TLFreeFunctionDeclaration& declaration, const std::string_view expected_name,
        const FunctionStorageClass expected_storage_class, const ConstantEvaluationKind expected_evaluation_kind,
        const bool expected_has_inline_definition, const std::size_t expected_function_parameter_count,
        const SourceReturnType expected_source_return_type, const std::size_t expected_template_parameter_count,
        const std::optional<TemplateSpecializationKind> expected_specialization_kind,
        const std::optional<ExpectedFunctionTemplateDetails>& expected_template_details) {
        const auto qualified_name = QualifiedName(expected_name);
        const auto expected_as_string = ExpectedAsString(
            expected_name, expected_storage_class, expected_evaluation_kind, expected_has_inline_definition,
            expected_function_parameter_count, expected_source_return_type, expected_template_parameter_count,
            expected_specialization_kind);
        const auto expected_inline_definition =
            expected_has_inline_definition
                ? std::optional<std::string_view>{InlineDefinition(expected_source_return_type)}
                : std::nullopt;

        const auto expect = [&](const std::initializer_list<ExpectedFunctionParameter> parameters) {
            UEMeta::Testing::ExpectFunctionCommon(
                declaration.common(), expected_name, qualified_name, FreeFunctionSourcePath(), FUNCTION_KIND_FREE,
                expected_as_string,
                EffectiveReturnType(expected_source_return_type, expected_has_inline_definition,
                                    expected_specialization_kind),
                expected_storage_class, expected_evaluation_kind, std::nullopt, expected_inline_definition,
                expected_template_details, parameters, std::nullopt);
        };

        const auto first_qualified_name = qualified_name + "::First";
        const auto second_qualified_name = qualified_name + "::Second";
        switch (expected_function_parameter_count) {
        case 0:
            expect({});
            break;
        case 1:
            expect({{"First", first_qualified_name, "int", "", "int First"}});
            break;
        case 2:
            expect({{"First", first_qualified_name, "int", "", "int First"},
                    {"Second", second_qualified_name, "long", "", "long Second"}});
            break;
        default:
            FAIL() << "Unsupported function parameter count " << expected_function_parameter_count;
        }
    }

    void ExpectFreeFunctionDeclaration(const std::string_view expected_name,
                                       const FunctionStorageClass expected_storage_class,
                                       const ConstantEvaluationKind expected_evaluation_kind,
                                       const bool expected_has_inline_definition,
                                       const std::size_t expected_function_parameter_count,
                                       const SourceReturnType expected_source_return_type,
                                       const std::size_t expected_template_parameter_count,
                                       const std::optional<TemplateSpecializationKind> expected_specialization_kind) {
        const auto key = FreeFunctionKey(expected_name, expected_specialization_kind);
        const auto& declarations = FreeFunctionDeclarations();
        const auto found = declarations.find(key);
        ASSERT_NE(found, declarations.end()) << "No parser output for free function " << key;

        SCOPED_TRACE(key);
        const auto& declaration = found->second;
        ASSERT_TRUE(declaration.has_metadata());
        ASSERT_TRUE(declaration.has_common());

        UEMeta::Testing::ExpectDeclarationMetadata(declaration.metadata(), expected_name, QualifiedName(expected_name),
                                                   FreeFunctionSourcePath(), declaration.metadata().occurrence_index(),
                                                   false);

        if (!expected_specialization_kind) {
            ASSERT_EQ(expected_template_parameter_count, 0);
            ExpectFunctionCommonWithParameters(
                declaration, expected_name, expected_storage_class, expected_evaluation_kind,
                expected_has_inline_definition, expected_function_parameter_count, expected_source_return_type,
                expected_template_parameter_count, expected_specialization_kind, std::nullopt);
            return;
        }

        const auto qualified_name = QualifiedName(expected_name);
        const auto arguments = TemplateArguments(expected_template_parameter_count, expected_specialization_kind);
        if (expected_template_parameter_count == 1) {
            const ExpectedFunctionTemplateDetails template_details{
                *expected_specialization_kind,
                qualified_name,
                {{"FirstType", "FirstType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename FirstType"}},
                arguments};
            ExpectFunctionCommonWithParameters(
                declaration, expected_name, expected_storage_class, expected_evaluation_kind,
                expected_has_inline_definition, expected_function_parameter_count, expected_source_return_type,
                expected_template_parameter_count, expected_specialization_kind, template_details);
            return;
        }

        ASSERT_EQ(expected_template_parameter_count, 2);
        const ExpectedFunctionTemplateDetails template_details{
            *expected_specialization_kind,
            qualified_name,
            {{"FirstType", "FirstType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename FirstType"},
             {"SecondType", "SecondType", TEMPLATE_PARAMETER_KIND_TYPENAME, "typename SecondType"}},
            arguments};
        ExpectFunctionCommonWithParameters(declaration, expected_name, expected_storage_class, expected_evaluation_kind,
                                           expected_has_inline_definition, expected_function_parameter_count,
                                           expected_source_return_type, expected_template_parameter_count,
                                           expected_specialization_kind, template_details);
    }

    void ExpectNonTemplateFreeFunction(const std::string_view expected_name,
                                       const FunctionStorageClass expected_storage_class,
                                       const ConstantEvaluationKind expected_evaluation_kind,
                                       const bool expected_has_inline_definition,
                                       const std::size_t expected_function_parameter_count,
                                       const SourceReturnType expected_source_return_type) {
        ExpectFreeFunctionDeclaration(expected_name, expected_storage_class, expected_evaluation_kind,
                                      expected_has_inline_definition, expected_function_parameter_count,
                                      expected_source_return_type, 0, std::nullopt);
    }

    void ExpectTemplatedFreeFunction(const std::string_view expected_name,
                                     const FunctionStorageClass expected_storage_class,
                                     const ConstantEvaluationKind expected_evaluation_kind,
                                     const bool expected_has_inline_definition,
                                     const std::size_t expected_function_parameter_count,
                                     const SourceReturnType expected_source_return_type,
                                     const std::size_t expected_template_parameter_count,
                                     const TemplateSpecializationKind expected_specialization_kind) {
        ExpectFreeFunctionDeclaration(expected_name, expected_storage_class, expected_evaluation_kind,
                                      expected_has_inline_definition, expected_function_parameter_count,
                                      expected_source_return_type, expected_template_parameter_count,
                                      expected_specialization_kind);
    }
} // namespace

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedNoneWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedNoneWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternNoneWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstexprWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstexprWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternConstevalWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternConstevalWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN_C,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN_C,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN_C,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_NONE, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCNoneWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN_C,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstexprWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstexprWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateExternCConstevalWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateExternCConstevalWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_EXTERN_C, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticNoneWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                  CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstexprWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstexprWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithoutDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithoutDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionZeroParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionZeroParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionZeroParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionZeroParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionZeroParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionZeroParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionOneParameterAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionOneParameterAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionOneParameterVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionOneParameterVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionOneParameterInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionOneParameterInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionTwoParametersAuto_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionTwoParametersAuto",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Auto);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionTwoParametersVoid_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionTwoParametersVoid",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Void);
}

TEST(FreeFunctionTests, NoTemplateStaticConstevalWithDefinitionTwoParametersInt_NoTemplate) {
    ExpectNonTemplateFreeFunction("NoTemplateStaticConstevalWithDefinitionTwoParametersInt",
                                  FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                  SourceReturnType::Int);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateOneExternConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 1,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateOneStaticConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateOneStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 1, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests,
     TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_EXTERN,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDeclaration) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
}

TEST(FreeFunctionTests, TemplateTwoExternConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoExternConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_EXTERN, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, false, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionZeroParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 0, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersAuto", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Auto, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersVoid", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Void, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_NONE, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticNoneWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticNoneWithDefinitionTwoParametersInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_NONE, true, 2, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEXPR, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstexprWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstexprWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEXPR, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, false, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionZeroParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionZeroParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 0,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 1,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionOneParameterInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionOneParameterInt", FUN_VAR_STORAGE_CLASS_STATIC,
                                CONSTANT_EVALUATION_CONSTEVAL, true, 1, SourceReturnType::Int, 2,
                                TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Auto, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Void, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersInt_Primary) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_NONE);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersInt_Implicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_IMPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersInt_Explicit) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_UNSPECIFIED, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT);
}

TEST(FreeFunctionTests, TemplateTwoStaticConstevalWithDefinitionTwoParametersInt_ExplicitInstantiationDefinition) {
    ExpectTemplatedFreeFunction("TemplateTwoStaticConstevalWithDefinitionTwoParametersInt",
                                FUN_VAR_STORAGE_CLASS_STATIC, CONSTANT_EVALUATION_CONSTEVAL, true, 2,
                                SourceReturnType::Int, 2, TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
}

TEST(FreeFunctionCoverageTests, AccountsForEveryTargetDeclaration) {
    constexpr std::size_t expected_declaration_count = 1404;
    constexpr std::size_t implicit_instantiation_anchor_count = 54;

    const auto& declarations = FreeFunctionDeclarations();
    ASSERT_EQ(declarations.size(), expected_declaration_count);

    std::vector<bool> occurrence_indices(expected_declaration_count + implicit_instantiation_anchor_count, false);
    for (const auto& [key, declaration] : declarations) {
        SCOPED_TRACE(key);
        const auto occurrence_index = declaration.metadata().occurrence_index();
        ASSERT_LT(occurrence_index, occurrence_indices.size());
        EXPECT_FALSE(occurrence_indices[occurrence_index]) << "Duplicate occurrence index " << occurrence_index;
        occurrence_indices[occurrence_index] = true;
    }

    EXPECT_EQ(std::ranges::count(occurrence_indices, false), implicit_instantiation_anchor_count);
}
