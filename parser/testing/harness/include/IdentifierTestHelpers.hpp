#pragma once

#include <gtest/gtest.h>

#include "VersionedProtoTestHelpers.hpp"

#include <filesystem>
#include <string_view>

namespace UEMeta::Testing {
    inline void ExpectIdentifier(
        const ParseResult::Identifier& identifier,
        const std::string_view expected_name,
        const std::string_view expected_qualified_name,
        const std::filesystem::path& expected_file_path) {
        EXPECT_EQ(identifier.name(), expected_name);
        EXPECT_EQ(identifier.qualified_name(), expected_qualified_name);
        EXPECT_EQ(VersionedValue(identifier.file_path()), expected_file_path.string());

        if (expected_qualified_name.empty()) {
            EXPECT_EQ(identifier.scope_size(), 0);
            return;
        }

        constexpr std::string_view scope_separator = "::";
        std::size_t scope_index = 0;
        std::size_t scope_begin = 0;
        while (scope_begin <= expected_qualified_name.size()) {
            const auto scope_end = expected_qualified_name.find(scope_separator, scope_begin);
            const auto scope = expected_qualified_name.substr(scope_begin, scope_end - scope_begin);

            ASSERT_LT(scope_index, static_cast<std::size_t>(identifier.scope_size()));
            EXPECT_EQ(identifier.scope(static_cast<int>(scope_index)), scope);
            ++scope_index;

            if (scope_end == std::string_view::npos) {
                break;
            }
            scope_begin = scope_end + scope_separator.size();
        }

        EXPECT_EQ(identifier.scope_size(), scope_index);
    }
}
