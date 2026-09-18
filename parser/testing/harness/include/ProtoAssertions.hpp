#pragma once

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include "TopLevel.pb.h"
#include "UEMeta/wrappers/Utility.hpp"
#include "boost/hash2/hash_append.hpp"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace UEMeta::Testing {
    // Build expected schema values independently of production serialization helpers.
    template <typename Message, typename Value>
    Message versioned(const Value& value) {
        Message message;
        auto*   version = message.add_versions();
        version->add_source_versions("test-version");
        version->set_value(value);
        return message;
    }

    inline ParserTypes::VersionedBool boolean(bool value) {
        ParserTypes::VersionedBool message;
        if (value)
            message.add_true_versions("test-version");
        else
            message.add_false_versions("test-version");
        return message;
    }

    inline ParserTypes::TypeRef builtin(std::string_view name) {
        ParserTypes::TypeRef message;
        *message.mutable_type_name() = versioned<ParserTypes::VersionedString>(name);
        message.set_is_builtin_or_template(true);
        return message;
    }

    // Expected messages are written by the test, not serialized by another wrapper.
    // Compare all fields, including optional presence, oneof alternatives and repeated-field order.
    template <typename Message>
    Message proto(std::string_view text) {
        Message message;
        EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(std::string{text}, &message)) << text;
        return message;
    }

    template <typename Message>
    void expectProto(const Message& actual, const Message& expected) {
        google::protobuf::util::MessageDifferencer differencer;
        std::string                                differences;
        differencer.ReportDifferencesToString(&differences);
        EXPECT_TRUE(differencer.Compare(expected, actual)) << differences;
    }

    inline Hash variableId(std::string_view signature) {
        boost::hash2::xxh3_128 hasher;
        hasher.update(signature.data(), signature.size());
        return Hash{hasher};
    }

    inline Hash enumId(std::string_view name) {
        boost::hash2::xxh3_128 hasher;
        boost::hash2::hash_append(hasher, boost::hash2::little_endian_flavor{}, name);
        return Hash{hasher};
    }

    inline void expectId(const ParserTypes::Hash& actual, const Hash& expected) {
        EXPECT_EQ(actual.a(), expected.a);
        EXPECT_EQ(actual.b(), expected.b);
    }

    inline ParserTypes::DeclarationMetadata metadata(std::string_view name, const Hash& identity, uint64_t occurrence,
                                                     std::string_view documentation = "", std::string_view file = "wrapper_fixture.cpp") {
        ParserTypes::DeclarationMetadata expected;
        expected.set_qualified_name(name);
        expected.mutable_decl_id()->set_a(identity.a);
        expected.mutable_decl_id()->set_b(identity.b);
        auto* source = expected.mutable_file_path()->add_versions();
        source->add_source_versions("test-version");
        source->set_value(file);
        auto* index = expected.mutable_occurrence_index()->add_versions();
        index->add_source_versions("test-version");
        index->set_value(occurrence);
        if (!documentation.empty()) {
            auto* doc = expected.mutable_documentation()->add_versions();
            doc->add_source_versions("test-version");
            doc->set_value(documentation);
        }
        return expected;
    }

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
        EXPECT_TRUE(metadata.has_qualified_name());
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
