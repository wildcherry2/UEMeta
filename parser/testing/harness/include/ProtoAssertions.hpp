#pragma once

#include <gtest/gtest.h>
#include <string_view>
#include "TopLevel.pb.h"

namespace UEMeta::Testing {
    template <typename Message, typename Value>
    void expectVersioned(const Message& message, const Value& expected) {
        ASSERT_EQ(message.versions_size(), 1);
        const auto& version = message.versions(0);
        ASSERT_EQ(version.source_versions_size(), 1);
        EXPECT_EQ(version.source_versions(0), "test-version");
        EXPECT_EQ(version.value(), expected);
    }

    inline void expectFalse(const ParserTypes::VersionedBool& message) {
        EXPECT_EQ(message.true_versions_size(), 0);
        ASSERT_EQ(message.false_versions_size(), 1);
        EXPECT_EQ(message.false_versions(0), "test-version");
    }

    inline void expectBuiltinType(const ParserTypes::VersionedTypeRefOrAnon& message, std::string_view name) {
        ASSERT_EQ(message.versions_size(), 1);
        const auto& version = message.versions(0);
        ASSERT_EQ(version.source_versions_size(), 1);
        EXPECT_EQ(version.source_versions(0), "test-version");
        ASSERT_TRUE(version.value().has_type_ref());
        const auto& type = version.value().type_ref();
        EXPECT_EQ(type.id_case(), ParserTypes::TypeRef::kIsBuiltinOrTemplate);
        EXPECT_TRUE(type.is_builtin_or_template());
        expectVersioned(type.type_name(), name);
    }

    inline void expectMetadata(const ParserTypes::DeclarationMetadata& metadata, std::string_view name,
                               std::string_view source_file = "wrapper_fixture.cpp") {
        EXPECT_EQ(metadata.qualified_name(), name);
        EXPECT_FALSE(metadata.has_is_anonymous());
        ASSERT_TRUE(metadata.has_decl_id());
        EXPECT_TRUE(metadata.decl_id().a() != 0 || metadata.decl_id().b() != 0);
        expectVersioned(metadata.file_path(), source_file);
        ASSERT_EQ(metadata.occurrence_index().versions_size(), 1);
        const auto& occurrence = metadata.occurrence_index().versions(0);
        ASSERT_EQ(occurrence.source_versions_size(), 1);
        EXPECT_EQ(occurrence.source_versions(0), "test-version");
    }
} // namespace UEMeta::Testing
