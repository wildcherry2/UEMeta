#pragma once

#include "IdentifierTestHelpers.hpp"

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UEMeta::Testing {
    struct ExpectedTemplateParameter {
        std::string_view name;
        std::string_view qualified_name;
        ParseResult::TemplateParameterKind kind;
        std::string_view as_string;
        std::optional<std::string_view> type{};
        std::optional<bool> is_parameter_pack{};
        std::optional<std::string_view> default_value{};
        std::vector<ExpectedTemplateParameter> parameters{};
    };

    namespace Detail {
        inline void ExpectTemplateParameter(
            const ParseResult::TemplateParameter& parameter,
            const ExpectedTemplateParameter& expected,
            const std::filesystem::path& expected_file_path) {
            ASSERT_TRUE(parameter.has_identifier());
            ExpectIdentifier(
                parameter.identifier(),
                expected.name,
                expected.qualified_name,
                expected_file_path);

            EXPECT_EQ(parameter.kind(), expected.kind);
            EXPECT_EQ(parameter.as_string(), expected.as_string);

            EXPECT_EQ(parameter.has_type(), expected.type.has_value());
            if (expected.type) {
                EXPECT_EQ(parameter.type(), *expected.type);
            }

            EXPECT_EQ(parameter.has_is_parameter_pack(), expected.is_parameter_pack.has_value());
            if (expected.is_parameter_pack) {
                EXPECT_EQ(parameter.is_parameter_pack(), *expected.is_parameter_pack);
            }

            EXPECT_EQ(parameter.has_default_value(), expected.default_value.has_value());
            if (expected.default_value) {
                EXPECT_EQ(parameter.default_value(), *expected.default_value);
            }

            ASSERT_EQ(parameter.parameters_size(), expected.parameters.size());
            for (std::size_t index = 0; index < expected.parameters.size(); ++index) {
                ExpectTemplateParameter(
                    parameter.parameters(static_cast<int>(index)),
                    expected.parameters[index],
                    expected_file_path);
            }
        }

        inline std::string TemplateArgumentsAsString(const ParseResult::TemplateDetails& details) {
            std::string result;
            for (int index = 0; index < details.arguments_size(); ++index) {
                if (!result.empty()) {
                    result += ", ";
                }
                result += details.arguments(index);
            }
            return result;
        }
    }

    inline void ExpectTemplateDetails(
        const ParseResult::TemplateDetails& details,
        const ParseResult::TemplateSpecializationKind expected_specialization_kind,
        const std::string_view expected_primary_template_qualified_name,
        const std::filesystem::path& expected_file_path,
        const std::initializer_list<ExpectedTemplateParameter> expected_parameters,
        const std::string_view expected_arguments) {
        EXPECT_TRUE(details.has_specialization_kind());
        EXPECT_EQ(details.specialization_kind(), expected_specialization_kind);
        EXPECT_TRUE(details.has_primary_template_qualified_name());
        EXPECT_EQ(details.primary_template_qualified_name(), expected_primary_template_qualified_name);
        EXPECT_EQ(Detail::TemplateArgumentsAsString(details), expected_arguments);

        ASSERT_EQ(details.parameters_size(), expected_parameters.size());
        int index = 0;
        for (const auto& expected_parameter : expected_parameters) {
            Detail::ExpectTemplateParameter(
                details.parameters(index++),
                expected_parameter,
                expected_file_path);
        }
    }
}
