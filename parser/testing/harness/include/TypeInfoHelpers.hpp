#pragma once

#include <gtest/gtest.h>

#include "VersionedProtoTestHelpers.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace UEMeta::Testing {
    struct ExpectedTypeInfo {
        std::string_view type;
        std::string_view underlying_type;
        std::optional<bool> is_templated_type{false};
        std::optional<std::filesystem::path> source_path{};
    };

    inline void ExpectTypeInfo(
        const ParseResult::TypeInfo& type_info,
        const ExpectedTypeInfo& expected) {
        EXPECT_EQ(VersionedValue(type_info.type()), expected.type);
        EXPECT_EQ(VersionedValue(type_info.underlying_type()), expected.underlying_type);

        EXPECT_EQ(type_info.has_is_templated_type(), expected.is_templated_type.has_value());
        if (expected.is_templated_type) {
            EXPECT_EQ(type_info.is_templated_type(), *expected.is_templated_type);
        }

        EXPECT_TRUE(type_info.has_source_path_hash());
        const auto expected_source_path_hash = expected.source_path
            ? std::hash<std::string>{}(expected.source_path->string())
            : 0;
        EXPECT_EQ(VersionedValue(type_info.source_path_hash()), expected_source_path_hash);
    }
}
