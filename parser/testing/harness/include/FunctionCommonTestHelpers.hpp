#pragma once

#include "IdentifierTestHelpers.hpp"
#include "TemplateDetailsTestHelpers.hpp"
#include "TypeInfoHelpers.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace UEMeta::Testing {
    struct ExpectedFunctionParameter {
        std::string_view name;
        std::string_view qualified_name;
        ExpectedTypeInfo type_info;
        std::string_view default_value;
        std::string_view as_string;
    };

    struct ExpectedFunctionTemplateDetails {
        ParseResult::TemplateSpecializationKind specialization_kind;
        std::string_view primary_template_qualified_name;
        std::initializer_list<ExpectedTemplateParameter> parameters;
        std::string_view arguments;
    };

    inline void ExpectFunctionCommon(
        const ParseResult::FunctionCommon& common,
        const std::string_view expected_name,
        const std::string_view expected_qualified_name,
        const std::filesystem::path& expected_file_path,
        const ParseResult::FunctionKind expected_kind,
        const std::string_view expected_as_string,
        const std::optional<ExpectedTypeInfo>& expected_return_type,
        const std::optional<ParseResult::FunctionStorageClass> expected_storage_class,
        const std::optional<ParseResult::ConstantEvaluationKind> expected_consteval_kind,
        const std::optional<bool> expected_is_explicit,
        const std::optional<std::string_view> expected_inline_definition,
        const std::optional<ExpectedFunctionTemplateDetails>& expected_template_details,
        const std::initializer_list<ExpectedFunctionParameter> expected_parameters,
        const std::optional<ParseResult::FunctionDefinitionKind> expected_definition_kind) {
        ASSERT_TRUE(common.has_identifier());
        ExpectIdentifier(
            common.identifier(),
            expected_name,
            expected_qualified_name,
            expected_file_path);

        EXPECT_EQ(common.kind(), expected_kind);
        static_cast<void>(expected_as_string);

        EXPECT_EQ(common.has_return_type(), expected_return_type.has_value());
        if (expected_return_type) {
            ExpectTypeInfo(common.return_type(), *expected_return_type);
        }

        EXPECT_TRUE(common.has_storage_class());
        EXPECT_EQ(
            VersionedValue(common.storage_class()),
            expected_storage_class.value_or(ParseResult::FUN_VAR_STORAGE_CLASS_UNSPECIFIED));

        EXPECT_TRUE(common.has_consteval_kind());
        EXPECT_EQ(
            VersionedValue(common.consteval_kind()),
            expected_consteval_kind.value_or(ParseResult::CONSTANT_EVALUATION_NONE));

        EXPECT_EQ(common.has_is_explicit(), expected_is_explicit.has_value());
        if (expected_is_explicit) {
            EXPECT_EQ(common.is_explicit(), *expected_is_explicit);
        }

        EXPECT_TRUE(common.has_inline_definition());
        EXPECT_EQ(
            VersionedValue(common.inline_definition()),
            expected_inline_definition.value_or(std::string_view{}));

        EXPECT_EQ(common.has_template_details(), expected_template_details.has_value());
        if (common.has_template_details() && expected_template_details) {
            ExpectTemplateDetails(
                common.template_details(),
                expected_template_details->specialization_kind,
                expected_template_details->primary_template_qualified_name,
                expected_file_path,
                expected_template_details->parameters,
                expected_template_details->arguments);
        }

        ASSERT_EQ(common.parameters_size(), expected_parameters.size());
        std::size_t parameter_index = 0;
        for (const auto& expected_parameter : expected_parameters) {
            SCOPED_TRACE(parameter_index);
            const auto& parameter = common.parameters(static_cast<int>(parameter_index));

            EXPECT_EQ(
                parameter.occurrence_index(),
                static_cast<std::uint64_t>(parameter_index));
            ASSERT_TRUE(parameter.has_identifier());
            ExpectIdentifier(
                parameter.identifier(),
                expected_parameter.name,
                std::string{expected_qualified_name} + "::" + std::string{expected_parameter.name},
                expected_file_path);
            static_cast<void>(expected_parameter.qualified_name);
            ASSERT_TRUE(parameter.has_type_info());
            ExpectTypeInfo(parameter.type_info(), expected_parameter.type_info);
            EXPECT_EQ(VersionedValue(parameter.default_value()), expected_parameter.default_value);

            ++parameter_index;
        }

        EXPECT_EQ(common.has_definition_kind(), expected_definition_kind.has_value());
        if (expected_definition_kind) {
            EXPECT_EQ(common.definition_kind(), *expected_definition_kind);
        }
    }
}
