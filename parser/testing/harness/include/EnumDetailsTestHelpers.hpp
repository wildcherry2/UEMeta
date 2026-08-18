#pragma once

#include <gtest/gtest.h>

#include "parser.pb.h"

#include <string_view>

namespace UEMeta::Testing {
    inline void ExpectEnumDetails(
        const ParseResult::EnumDetails& details,
        const ParseResult::EnumScope expected_scope,
        const std::string_view expected_underlying_type = {}) {
        EXPECT_TRUE(details.has_scope());
        EXPECT_EQ(details.scope(), expected_scope);

        if (!expected_underlying_type.empty()) {
            EXPECT_TRUE(details.has_underlying_type());
            EXPECT_EQ(details.underlying_type(), expected_underlying_type);
        }
    }
}
