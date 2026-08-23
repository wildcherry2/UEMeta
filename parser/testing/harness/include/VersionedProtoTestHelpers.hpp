#pragma once

#include <filesystem>

#include <gtest/gtest.h>

#include "TopLevel.pb.h"

namespace ParseResult = ParserTypes;

namespace UEMeta::Testing {
    inline std::string ExpectedSourceVersion() {
        return std::filesystem::path{UEMETA_TEST_OUTPUT_DIR}.filename().string();
    }

    template <typename VersionedMessage>
    decltype(auto) VersionedValue(const VersionedMessage& message) {
        EXPECT_EQ(message.versions_size(), 1);
        const auto& item = message.versions(0);
        EXPECT_EQ(item.source_versions_size(), 1);
        EXPECT_EQ(item.source_versions(0), ExpectedSourceVersion());
        return item.value();
    }

    inline bool VersionedValue(const ParserTypes::VersionedBool& message) {
        EXPECT_EQ(message.true_versions_size() + message.false_versions_size(), 1);
        if (!message.true_versions().empty()) {
            EXPECT_EQ(message.true_versions(0), ExpectedSourceVersion());
            return true;
        }
        EXPECT_EQ(message.false_versions(0), ExpectedSourceVersion());
        return false;
    }
}
