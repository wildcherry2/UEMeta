#pragma once

#include "IdentifierTestHelpers.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace UEMeta::Testing {
    inline void ExpectDeclarationMetadata(
        const ParseResult::DeclarationMetadata& metadata,
        const std::string_view expected_name,
        const std::string_view expected_qualified_name,
        const std::filesystem::path& expected_file_path,
        const std::uint32_t expected_occurrence_index,
        const bool expected_is_anonymous) {
        ASSERT_TRUE(metadata.has_identifier());
        ExpectIdentifier(
            metadata.identifier(),
            expected_name,
            expected_qualified_name,
            expected_file_path);
        EXPECT_FALSE(metadata.identifier().qualified_name().empty());
        EXPECT_NE(metadata.identifier().qualified_name_hash(), 0);

        EXPECT_EQ(VersionedValue(metadata.occurrence_index()), expected_occurrence_index);
        EXPECT_TRUE(metadata.has_is_anonymous());
        EXPECT_EQ(metadata.is_anonymous(), expected_is_anonymous);
    }
}
