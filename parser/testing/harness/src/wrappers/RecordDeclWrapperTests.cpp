#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "ProtoAssertions.hpp"
#include "UEMeta/clang/wrappers/RecordDeclWrapper.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "WrapperTest.hpp"
#include "clang/AST/DeclTemplate.h"

namespace {
    using UEMeta::RecordDeclWrapper;
    using namespace UEMeta::Testing;
    using Record      = ParserTypes::TLRecordDeclaration;
    using Variables   = std::vector<ParserTypes::TLGlobalVariableDeclaration*>;
    using RecordError = UEMeta::DeclException<clang::RecordDecl>;

    UEMeta::Hash recordId(std::string_view name, std::string_view template_signature = "") {
        boost::hash2::xxh3_128 hasher;
        boost::hash2::hash_append(hasher, boost::hash2::little_endian_flavor{}, name);
        hasher.update(template_signature.data(), template_signature.size());
        return UEMeta::Hash{hasher};
    }

    ParserTypes::TypeRef reference(std::string_view name, const UEMeta::Hash& id) {
        auto type = builtin(name);
        id.putProtoHash(type.mutable_decl_id());
        return type;
    }

    ParserTypes::VersionedTypeRefOrAnon fieldType(const ParserTypes::TypeRef& type) {
        ParserTypes::VersionedTypeRefOrAnon result;
        *result.mutable_type_ref() = versionedRef(type);
        return result;
    }

    ParserTypes::Field field(std::optional<std::string_view> name, std::string_view type, std::optional<uint64_t> width,
                             std::optional<uint64_t> offset, uint64_t occurrence = 0,
                             ParserTypes::AccessSpecifier access = ParserTypes::ACCESS_SPECIFIER_PUBLIC) {
        ParserTypes::Field result;
        if (name)
            result.set_name(*name);
        *result.mutable_access()        = versioned<ParserTypes::VersionedAccessSpecifier>(access);
        *result.mutable_is_mutable()    = boolean(false);
        *result.mutable_is_bitfield()   = boolean(false);
        *result.mutable_storage_class() = versioned<ParserTypes::VersionedVariableStorageClass>(ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);
        *result.mutable_constant_evaluation_kind() = versioned<ParserTypes::VersionedConstantEvaluationKind>(ParserTypes::CONSTANT_EVALUATION_NONE);
        *result.mutable_type_ref()                 = fieldType(builtin(type));
        if (width)
            *result.mutable_bit_width() = versioned<ParserTypes::VersionedUint64>(*width);
        if (offset)
            *result.mutable_offset_bits() = versioned<ParserTypes::VersionedUint64>(*offset);
        *result.mutable_local_occurrence_index() = versioned<ParserTypes::VersionedUint64>(occurrence);
        return result;
    }

    ParserTypes::Field staticField(std::string_view name, std::string_view type, std::optional<uint64_t> width,
                                   ParserTypes::VariableStorageClass   storage    = ParserTypes::VAR_STORAGE_CLASS_STATIC,
                                   ParserTypes::ConstantEvaluationKind evaluation = ParserTypes::CONSTANT_EVALUATION_NONE) {
        auto result                                = field(name, type, width, std::nullopt);
        result.clear_local_occurrence_index();
        *result.mutable_storage_class()            = versioned<ParserTypes::VersionedVariableStorageClass>(storage);
        *result.mutable_constant_evaluation_kind() = versioned<ParserTypes::VersionedConstantEvaluationKind>(evaluation);
        return result;
    }

    Record record(std::string_view name, uint64_t occurrence, ParserTypes::RecordKind kind, std::optional<int64_t> size,
                  std::optional<int64_t> alignment, std::string_view signature = "", std::string_view documentation = "") {
        Record result;
        *result.mutable_metadata() = metadata(name, recordId(name, signature), occurrence, documentation);
        result.set_kind(kind);
        if (size)
            *result.mutable_size_bytes() = versioned<ParserTypes::VersionedInt64>(*size);
        if (alignment)
            *result.mutable_align_bytes() = versioned<ParserTypes::VersionedInt64>(*alignment);
        return result;
    }

    Record anonymousRecord(uint64_t occurrence, ParserTypes::RecordKind kind, std::optional<int64_t> size, std::optional<int64_t> alignment) {
        auto result = record("", occurrence, kind, size, alignment);
        result.mutable_metadata()->clear_qualified_name();
        result.mutable_metadata()->clear_decl_id();
        result.mutable_metadata()->set_is_anonymous(true);
        return result;
    }

    void nestedHash(Record& message, const UEMeta::Hash& id) {
        auto* hashes = message.mutable_nested_hashes();
        if (hashes->versions_size() == 0)
            hashes->add_versions()->add_source_versions("test-version");
        id.putProtoHash(hashes->mutable_versions(0)->add_value());
    }

    ParserTypes::BaseSpecifier base(const ParserTypes::TypeRef& type, ParserTypes::AccessSpecifier access, bool is_virtual,
                                    std::optional<uint64_t> offset, uint64_t occurrence = 0) {
        ParserTypes::BaseSpecifier result;
        *result.mutable_type_ref()   = type;
        *result.mutable_access()     = versioned<ParserTypes::VersionedAccessSpecifier>(access);
        *result.mutable_is_virtual() = boolean(is_virtual);
        *result.mutable_local_occurrence_index() = versioned<ParserTypes::VersionedUint64>(occurrence);
        if (offset)
            *result.mutable_offset() = versioned<ParserTypes::VersionedUint64>(*offset);
        return result;
    }

    ParserTypes::TemplateParameter typeParameter(std::string_view name, bool pack = false) {
        ParserTypes::TemplateParameter result;
        result.set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS);
        *result.mutable_type() = builtin(name);
        if (pack)
            result.set_is_parameter_pack(true);
        return result;
    }

    class RecordDeclWrapperTest : public WrapperTest {
    protected:
        std::vector<clang::RecordDecl*> parse(std::string_view code, const std::vector<std::string>& arguments = {}) {
            std::vector<clang::RecordDecl*> declarations;
            if (auto* context = parseCode(code, "wrapper_fixture.cpp", arguments))
                collect(context->getTranslationUnitDecl(), declarations);
            return declarations;
        }

        Record* serialize(const clang::RecordDecl* declaration) const {
            const auto  ir     = RecordDeclWrapper{declaration, arena}.toIntermediateRepresentation();
            const auto* result = std::get_if<Record*>(&ir);
            EXPECT_NE(result, nullptr);
            return result ? *result : nullptr;
        }

        static std::vector<std::filesystem::path> addedFiles(const std::vector<std::filesystem::path>& before) {
            const auto                         after = outputFiles();
            std::vector<std::filesystem::path> added;
            std::set_difference(after.begin(), after.end(), before.begin(), before.end(), std::back_inserter(added));
            return added;
        }

        static void expectRegistered(clang::RecordDecl* declaration, const UEMeta::Hash& expected) {
            const auto query = UEMeta::DeclDb::queryDeclIdentity(declaration);
            ASSERT_TRUE(std::holds_alternative<UEMeta::Hash>(query));
            EXPECT_EQ(std::get<UEMeta::Hash>(query), expected);
            EXPECT_EQ(UEMeta::DeclDb::queryDecl(expected), declaration);
        }

    private:
        static void collect(clang::DeclContext* context, std::vector<clang::RecordDecl*>& declarations) {
            for (auto* declaration : context->decls()) {
                if (declaration->isImplicit())
                    continue;
                if (auto* templated = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                    declaration = templated->getTemplatedDecl();
                if (auto* record_decl = llvm::dyn_cast<clang::RecordDecl>(declaration)) {
                    declarations.push_back(record_decl);
                    collect(record_decl, declarations);
                }
                else if (auto* space = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                    collect(space, declarations);
                else if (auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                    collect(linkage, declarations);
            }
        }
    };

    TEST_F(RecordDeclWrapperTest, EmptyRecordsPreserveKindsMetadataIdentityAndArena) {
        const auto records = parse("namespace Outer::Inner { /// Record documentation.\nstruct S {}; class C {}; union U {}; }");
        ASSERT_EQ(records.size(), 3u);
        const std::vector<std::string>             names{"::Outer::Inner::S", "::Outer::Inner::C", "::Outer::Inner::U"};
        const std::vector<ParserTypes::RecordKind> kinds{ParserTypes::RECORD_KIND_STRUCT, ParserTypes::RECORD_KIND_CLASS,
                                                         ParserTypes::RECORD_KIND_UNION};
        for (size_t i = 0; i < records.size(); ++i) {
            SCOPED_TRACE(names[i]);
            const auto* actual = serialize(records[i]);
            ASSERT_NE(actual, nullptr);
            EXPECT_EQ(actual->GetArena(), arena.get());
            expectProto(*actual, record(names[i], i, kinds[i], 1, 1, "", i == 0 ? "/// Record documentation." : ""));
            expectRegistered(records[i], recordId(names[i]));
        }
    }

    TEST_F(RecordDeclWrapperTest, WindowsLayoutPreservesAccessQualifiersDocumentationAndInitializers) {
        const auto records = parse(R"cpp(class Layout {
            char first;
        protected:
            /// Mutable counter.
            mutable int count = 6 * 7;
        public:
            const long value = 3;
            void* pointer;
        };)cpp");
        ASSERT_EQ(records.size(), 1u);
        auto expected                    = record("::Layout", 0, ParserTypes::RECORD_KIND_CLASS, 24, 8);
        *expected.add_fields()           = field("first", "char", 8, 0, 0, ParserTypes::ACCESS_SPECIFIER_PRIVATE);
        auto counter                     = field("count", "int", 32, 32, 1, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *counter.mutable_is_mutable()    = boolean(true);
        *counter.mutable_documentation() = versioned<ParserTypes::VersionedString>("/// Mutable counter.");
        *counter.mutable_default_value() = versioned<ParserTypes::VersionedString>("6 * 7");
        *expected.add_fields()           = counter;
        auto value                       = field("value", "const long", 32, 64, 2);
        *value.mutable_default_value()   = versioned<ParserTypes::VersionedString>("3");
        *expected.add_fields()           = value;
        *expected.add_fields()           = field("pointer", "void *", 64, 128, 3);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, OrdinaryIdentityIgnoresVersionedLayoutButDistinguishesOwningScopes) {
        const auto first   = parse("namespace N { struct Stable { int value; }; }");
        const auto changed = parse("namespace N { struct Stable { double value; }; }");
        const auto other   = parse("namespace Other { struct Stable { double value; }; }");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(changed.size(), 1u);
        ASSERT_EQ(other.size(), 1u);
        auto expected          = record("::N::Stable", 0, ParserTypes::RECORD_KIND_STRUCT, 4, 4);
        *expected.add_fields() = field("value", "int", 32, 0);
        expectProto(*serialize(first[0]), expected);
        expected               = record("::N::Stable", 1, ParserTypes::RECORD_KIND_STRUCT, 8, 8);
        *expected.add_fields() = field("value", "double", 64, 0);
        expectProto(*serialize(changed[0]), expected);
        expected               = record("::Other::Stable", 2, ParserTypes::RECORD_KIND_STRUCT, 8, 8);
        *expected.add_fields() = field("value", "double", 64, 0);
        expectProto(*serialize(other[0]), expected);
        EXPECT_NE(recordId("::N::Stable"), recordId("::Other::Stable"));
    }

    TEST_F(RecordDeclWrapperTest, UnionMembersOverlapWithoutReorderingOrInventingPaddingFields) {
        const auto records = parse("union Value { char small; double large; int values[3]; };");
        ASSERT_EQ(records.size(), 1u);
        auto expected          = record("::Value", 0, ParserTypes::RECORD_KIND_UNION, 16, 8);
        *expected.add_fields() = field("small", "char", 8, 0);
        *expected.add_fields() = field("large", "double", 64, 0, 1);
        *expected.add_fields() = field("values", "int[3]", 96, 0, 2);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, BitfieldsRetainUnnamedAndZeroWidthEntriesInSourceOrder) {
        const auto records = parse("struct Bits { unsigned : 1; unsigned a : 3; unsigned : 2; unsigned b : 4 = 7; unsigned : 0; char tail; };");
        ASSERT_EQ(records.size(), 1u);
        auto                                               expected = record("::Bits", 0, ParserTypes::RECORD_KIND_STRUCT, 8, 4);
        const std::vector<std::optional<std::string_view>> names{std::nullopt, "a", std::nullopt, "b", std::nullopt};
        const std::vector<uint64_t>                        widths{1, 3, 2, 4, 0}, offsets{0, 1, 4, 6, 32};
        for (size_t i = 0; i < names.size(); ++i) {
            auto expected_field                   = field(names[i], "unsigned int", widths[i], offsets[i], i);
            *expected_field.mutable_is_bitfield() = boolean(true);
            if (i == 3)
                *expected_field.mutable_default_value() = versioned<ParserTypes::VersionedString>("7");
            *expected.add_fields() = expected_field;
        }
        *expected.add_fields() = field("tail", "char", 8, 32, 5);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, StaticFieldsPreserveStorageConstantsAndIncompleteTypesWithoutOffsets) {
        const auto records = parse(R"cpp(struct Static {
            static int ordinary;
            static thread_local long tls;
            static constexpr int answer = 42;
            static const int values[];
            template<class T> static T dependent;
        };)cpp");
        ASSERT_EQ(records.size(), 1u);
        auto expected          = record("::Static", 0, ParserTypes::RECORD_KIND_STRUCT, 1, 1);
        *expected.add_fields() = staticField("ordinary", "int", 32);
        *expected.add_fields() = staticField("tls", "long", 32, ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL);
        auto answer = staticField("answer", "const int", 32, ParserTypes::VAR_STORAGE_CLASS_STATIC, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        *answer.mutable_default_value() = versioned<ParserTypes::VersionedString>("42");
        *expected.add_fields()          = answer;
        *expected.add_fields()          = staticField("values", "const int[]", std::nullopt);
        *expected.add_fields()          = staticField("dependent", "type-parameter-0-0", std::nullopt);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, SelfReferencesResolveBeforeFieldsAndKeepCanonicalDeclaratorSpelling) {
        const auto records = parse("namespace N { struct Node { using Alias = Node; const Alias* next; Alias& reference; Alias* links[2]; }; }");
        ASSERT_EQ(records.size(), 1u);
        auto expected = record("::N::Node", 0, ParserTypes::RECORD_KIND_STRUCT, 32, 8);
        // Clang prints arrays with a qualified element spelling but no leading global-scope token.
        const std::vector<std::string> names{"next", "reference", "links"}, types{"const ::N::Node *", "::N::Node &", "N::Node *[2]"};
        for (size_t i = 0; i < names.size(); ++i) {
            auto item                = field(names[i], types[i], i == 2 ? 128 : 64, i * 64, i);
            *item.mutable_type_ref() = fieldType(reference(types[i], recordId("::N::Node")));
            *expected.add_fields()   = item;
        }
        expectProto(*serialize(records[0]), expected);
        expectRegistered(records[0], recordId("::N::Node"));
    }

    TEST_F(RecordDeclWrapperTest, AnonymousStorageRaisesFieldsWithAccumulatedOffsetsAndOwningAccess) {
        const auto records = parse(R"cpp(class Outer {
            char prefix;
        protected:
            union {
                int first;
                struct { char inner_prefix; union { short leaf; unsigned short other; }; };
            };
        public:
            char suffix;
        };)cpp",
                                   {"-fms-extensions"});
        ASSERT_EQ(records.size(), 4u);
        auto expected          = record("::Outer", 0, ParserTypes::RECORD_KIND_CLASS, 12, 4);
        *expected.add_fields() = field("prefix", "char", 8, 0, 0, ParserTypes::ACCESS_SPECIFIER_PRIVATE);
        *expected.add_fields() = field("first", "int", 32, 32, 1, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *expected.add_fields() = field("inner_prefix", "char", 8, 32, 2, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *expected.add_fields() = field("leaf", "short", 16, 48, 3, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *expected.add_fields() = field("other", "unsigned short", 16, 48, 4, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *expected.add_fields() = field("suffix", "char", 8, 64, 5);
        const auto before      = outputFiles();
        expectProto(*serialize(records[0]), expected);
        for (size_t i = 1; i < records.size(); ++i)
            UEMeta::DeclDb::serializeIfNeeded(records[i]);
        EXPECT_EQ(outputFiles(), before); // Consumed anonymous records must not be serialized again.
    }

    TEST_F(RecordDeclWrapperTest, EmbeddedAnonymousRecordOwnsItsRaisedMembersAndSharesTheArena) {
        const auto records = parse("struct Outer { char prefix; struct { char tag; union { int x; float y; }; } value; };");
        ASSERT_EQ(records.size(), 3u);
        auto expected          = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, 12, 4);
        *expected.add_fields() = field("prefix", "char", 8, 0);
        auto value             = field("value", "", 64, 32, 1);
        auto embedded          = anonymousRecord(1, ParserTypes::RECORD_KIND_STRUCT, 8, 4);
        *embedded.add_fields() = field("tag", "char", 8, 0);
        *embedded.add_fields() = field("x", "int", 32, 32, 1);
        *embedded.add_fields() = field("y", "float", 32, 32, 2);
        value.mutable_type_ref()->clear_type_ref();
        *value.mutable_type_ref()->mutable_anon_record() = embedded;
        *expected.add_fields()                                                                 = value;
        const auto  before                                                                     = outputFiles();
        const auto* actual                                                                     = serialize(records[0]);
        expectProto(*actual, expected);
        EXPECT_EQ(actual->fields(1).type_ref().anon_record().GetArena(), arena.get());
        for (size_t i = 1; i < records.size(); ++i)
            UEMeta::DeclDb::serializeIfNeeded(records[i]);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, RaisedBitfieldsAccumulateBitOffsetsAndKeepPaddingInSourceOrder) {
        const auto records =
            parse("struct Outer { long long prefix; struct { short pad; unsigned short : 3; unsigned short leaf : 4; }; };", {"-fms-extensions"});
        ASSERT_EQ(records.size(), 2u);
        auto expected                  = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, 16, 8);
        *expected.add_fields()         = field("prefix", "long long", 64, 0);
        *expected.add_fields()         = field("pad", "short", 16, 64, 1);
        auto padding                   = field(std::nullopt, "unsigned short", 3, 80, 2);
        *padding.mutable_is_bitfield() = boolean(true);
        *expected.add_fields()         = padding;
        auto leaf                      = field("leaf", "unsigned short", 4, 83, 3);
        *leaf.mutable_is_bitfield()    = boolean(true);
        *expected.add_fields()         = leaf;
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, UnknownParentOffsetsDoNotBecomeZeroWhenAnonymousChildLayoutIsKnown) {
        const auto records = parse("struct Outer { char prefix; union { int x; short y; }; };");
        ASSERT_EQ(records.size(), 2u);
        records[0]->setInvalidDecl();
        auto expected          = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt);
        *expected.add_fields() = field("prefix", "char", std::nullopt, std::nullopt);
        // The valid nested union still supplies type widths, but no absolute instance offsets.
        *expected.add_fields() = field("x", "int", 32, std::nullopt, 1);
        *expected.add_fields() = field("y", "short", 16, std::nullopt, 2);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, EmbeddedAnonymousTypesAreFoundBehindPointerReferenceAndArrayLayers) {
        const auto records = parse("struct Outer { struct { int x; } *pointer; struct { short y; } (&reference)[2]; struct { char z; } array[3]; };");
        ASSERT_EQ(records.size(), 4u);
        auto                           expected = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, 24, 8);
        const std::vector<std::string> names{"pointer", "reference", "array"}, leaf_names{"x", "y", "z"}, types{"int", "short", "char"};
        const std::vector<uint64_t>    widths{64, 64, 24}, sizes{4, 2, 1};
        for (size_t i = 0; i < names.size(); ++i) {
            auto item              = field(names[i], "", widths[i], i * 64, i);
            auto embedded          = anonymousRecord(1 + i, ParserTypes::RECORD_KIND_STRUCT, sizes[i], sizes[i]);
            *embedded.add_fields() = field(leaf_names[i], types[i], sizes[i] * 8, 0);
            item.mutable_type_ref()->clear_type_ref();
            *item.mutable_type_ref()->mutable_anon_record() = embedded;
            *expected.add_fields()                                                                = item;
        }
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, TypedefNamedAnonymousRecordsHaveStandaloneIdentities) {
        const auto records = parse("typedef struct { int value; } Named; struct Owner { typedef struct { char tag; } Inner; Inner member; };");
        ASSERT_EQ(records.size(), 3u);
        auto expected          = record("::Named", 0, ParserTypes::RECORD_KIND_STRUCT, 4, 4);
        *expected.add_fields() = field("value", "int", 32, 0);
        expectProto(*serialize(records[0]), expected);
        expected = record("::Owner", 1, ParserTypes::RECORD_KIND_STRUCT, 1, 1);
        nestedHash(expected, recordId("::Owner::Inner"));
        auto member                = field("member", "::Owner::Inner", 8, 0);
        *member.mutable_type_ref() = fieldType(reference("::Owner::Inner", recordId("::Owner::Inner")));
        *expected.add_fields()     = member;
        expectProto(*serialize(records[1]), expected);
        expectRegistered(records[2], recordId("::Owner::Inner"));
    }

    TEST_F(RecordDeclWrapperTest, NestedTypesPublishOnlyDirectHashesAndAreNotEmittedTwice) {
        const auto records = parse(R"cpp(struct Outer {
            struct Inner { struct Leaf {}; };
            enum class Mode : unsigned char { Off, On };
            Inner value;
            Mode mode;
        };)cpp");
        ASSERT_EQ(records.size(), 3u);
        auto expected = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, 2, 1);
        nestedHash(expected, recordId("::Outer::Inner"));
        nestedHash(expected, enumId("::Outer::Mode"));
        auto value                = field("value", "::Outer::Inner", 8, 0);
        *value.mutable_type_ref() = fieldType(reference("::Outer::Inner", recordId("::Outer::Inner")));
        *expected.add_fields()    = value;
        auto mode                 = field("mode", "::Outer::Mode", 8, 8, 1);
        *mode.mutable_type_ref()  = fieldType(reference("::Outer::Mode", enumId("::Outer::Mode")));
        *expected.add_fields()    = mode;
        const auto before         = outputFiles();
        expectProto(*serialize(records[0]), expected);
        const auto files = addedFiles(before);
        ASSERT_EQ(files.size(), 3u);
        expectOutput(outputPath(metadata("::Outer::Inner", recordId("::Outer::Inner"), 1), "recordbin"));
        expectOutput(outputPath(metadata("::Outer::Inner::Leaf", recordId("::Outer::Inner::Leaf"), 2), "recordbin"));
        expectOutput(outputPath(metadata("::Outer::Mode", enumId("::Outer::Mode"), 3), "enumbin"));
        const auto after = outputFiles();
        for (auto* child : records[0]->decls()) {
            if (auto* r = llvm::dyn_cast<clang::RecordDecl>(child))
                UEMeta::DeclDb::serializeIfNeeded(r);
            if (auto* e = llvm::dyn_cast<clang::EnumDecl>(child))
                UEMeta::DeclDb::serializeIfNeeded(e);
        }
        EXPECT_EQ(outputFiles(), after);
    }

    TEST_F(RecordDeclWrapperTest, ForwardMembersRetainEncounterOrderAndLaterReferencesUseDefinitions) {
        const auto records = parse(R"cpp(struct Outer {
            struct Later;
            Later* before;
            struct Later { int value; };
            Later* after;
            struct Missing;
            Missing* unknown;
            enum class Mode : int;
            Mode early;
            enum class Mode : int { Ready };
            Mode late;
            enum class Unresolved : int;
        };)cpp");
        ASSERT_EQ(records.size(), 4u);
        auto expected = record("::Outer", 0, ParserTypes::RECORD_KIND_STRUCT, 32, 8);
        nestedHash(expected, recordId("::Outer::Later"));
        nestedHash(expected, enumId("::Outer::Mode"));
        auto early      = field("before", "::Outer::Later *", 64, 0);
        auto early_type = builtin("::Outer::Later *");
        early_type.set_forward_decl_index(1);
        *early.mutable_type_ref() = fieldType(early_type);
        *expected.add_fields()    = early;
        auto late                 = field("after", "::Outer::Later *", 64, 64, 1);
        *late.mutable_type_ref()  = fieldType(reference("::Outer::Later *", recordId("::Outer::Later")));
        *expected.add_fields()    = late;
        auto unknown              = field("unknown", "::Outer::Missing *", 64, 128, 2);
        *unknown.mutable_type_ref()->mutable_type_ref()->mutable_is_builtin_or_template() = boolean(false);
        *expected.add_fields() = unknown;
        early                  = field("early", "::Outer::Mode", 32, 192, 3);
        early_type             = builtin("::Outer::Mode");
        early_type.set_forward_decl_index(3);
        *early.mutable_type_ref() = fieldType(early_type);
        *expected.add_fields()    = early;
        late                      = field("late", "::Outer::Mode", 32, 224, 4);
        *late.mutable_type_ref()  = fieldType(reference("::Outer::Mode", enumId("::Outer::Mode")));
        *expected.add_fields()    = late;
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, AnonymousEnumeratorsBecomeStaticConstantsAtTheirSourcePosition) {
        const auto records = parse(R"cpp(class Owner {
            char first;
        protected:
            enum : unsigned short {
                /// An enum value.
                A = 3,
                /// Another enum value.
                B = 7 };
        public:
            char last;
        };)cpp");
        ASSERT_EQ(records.size(), 1u);
        auto expected          = record("::Owner", 0, ParserTypes::RECORD_KIND_CLASS, 2, 1);
        *expected.add_fields() = field("first", "char", 8, 0, 0, ParserTypes::ACCESS_SPECIFIER_PRIVATE);
        for (const auto name : {"A", "B"}) {
            auto constant =
                staticField(name, "unsigned short", std::nullopt, ParserTypes::VAR_STORAGE_CLASS_STATIC, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
            *constant.mutable_access() = versioned<ParserTypes::VersionedAccessSpecifier>(ParserTypes::ACCESS_SPECIFIER_PROTECTED);
            constant.set_is_anon_enum_value(true);
            *constant.mutable_default_value() = versioned<ParserTypes::VersionedString>(std::string_view{name} == "A" ? "3" : "7");
            *constant.mutable_documentation() =
                versioned<ParserTypes::VersionedString>(std::string_view{name} == "A" ? "/// An enum value." : "/// Another enum value.");
            *expected.add_fields() = constant;
        }
        *expected.add_fields() = field("last", "char", 8, 8, 1);
        const auto before      = outputFiles();
        expectProto(*serialize(records[0]), expected);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, EmbeddedEnumBelongsToItsFieldAndIsNotRaisedOrWrittenSeparately) {
        const auto records = parse("struct Owner { enum : unsigned char { A = 2, B } value; };");
        ASSERT_EQ(records.size(), 1u);
        auto expected = record("::Owner", 0, ParserTypes::RECORD_KIND_STRUCT, 1, 1);
        auto value    = field("value", "", 8, 0);
        auto embedded = proto<ParserTypes::TLEnumDeclaration>(R"pb(
            underlying_type { versions { source_versions: "test-version" value: "unsigned char" } }
            scope: ENUM_SCOPE_UNSCOPED
            enumerators { name: "A" value { versions { source_versions: "test-version" value: "2" } } }
            enumerators { name: "B" value { versions { source_versions: "test-version" value: "3" } } }
        )pb");
        // EnumDeclWrapper's current embedded representation includes Clang's source-qualified anonymous spelling.
        const std::string name                                                               = "::Owner::(unnamed enum at wrapper_fixture.cpp:1:16)";
        *embedded.mutable_metadata()                                                         = metadata(name, enumId(name), 1);
        value.mutable_type_ref()->clear_type_ref();
        *value.mutable_type_ref()->mutable_anon_enum() = embedded;
        *expected.add_fields()                                                               = value;
        const auto  before                                                                   = outputFiles();
        const auto* actual                                                                   = serialize(records[0]);
        expectProto(*actual, expected);
        ASSERT_EQ(actual->fields_size(), 1);
        EXPECT_EQ(actual->fields(0).type_ref().anon_enum().GetArena(), arena.get());
        for (auto* member : records[0]->decls())
            if (auto* e = llvm::dyn_cast<clang::EnumDecl>(member))
                UEMeta::DeclDb::serializeIfNeeded(e);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, AnonymousStructEnumeratorsInheritOwningAccessWithoutConsumingStorage) {
        const auto records = parse("class Owner { protected: struct { enum { Count = 3 }; int value; }; };", {"-fms-extensions"});
        ASSERT_EQ(records.size(), 2u);
        auto expected = record("::Owner", 0, ParserTypes::RECORD_KIND_CLASS, 4, 4);
        auto constant = staticField("Count", "int", std::nullopt, ParserTypes::VAR_STORAGE_CLASS_STATIC, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        constant.set_is_anon_enum_value(true);
        *constant.mutable_access()        = versioned<ParserTypes::VersionedAccessSpecifier>(ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        *constant.mutable_default_value() = versioned<ParserTypes::VersionedString>("3");
        *expected.add_fields()            = constant;
        *expected.add_fields()            = field("value", "int", 32, 0, 0, ParserTypes::ACCESS_SPECIFIER_PROTECTED);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, GlobalAnonymousUnionProducesOrderedStaticVariablesWithOuterNames) {
        const auto records = parse(R"cpp(namespace N { static union {
            /// Global union value.
            const int answer = 42;
            union { long alternate; short other; };
            unsigned : 2;
        }; })cpp",
                                   {"-fms-extensions"});
        ASSERT_EQ(records.size(), 2u);
        const auto ir = RecordDeclWrapper{records[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        const auto& values = std::get<Variables>(ir);
        ASSERT_EQ(values.size(), 3u);
        const std::vector<std::string> names{"::N::answer", "::N::alternate", "::N::other"}, types{"const int", "long", "short"};
        for (size_t i = 0; i < values.size(); ++i) {
            SCOPED_TRACE(names[i]);
            ParserTypes::TLGlobalVariableDeclaration expected;
            *expected.mutable_metadata()      = metadata(names[i], variableId(names[i]), i, i == 0 ? "/// Global union value." : "");
            *expected.mutable_type_ref()      = fieldType(builtin(types[i]));
            *expected.mutable_storage_class() = versioned<ParserTypes::VersionedVariableStorageClass>(ParserTypes::VAR_STORAGE_CLASS_STATIC);
            *expected.mutable_constant_evaluation_kind() =
                versioned<ParserTypes::VersionedConstantEvaluationKind>(ParserTypes::CONSTANT_EVALUATION_NONE);
            expected.set_is_anon_union_value(true);
            if (i == 0)
                *expected.mutable_default_value() = versioned<ParserTypes::VersionedString>("42");
            expectProto(*values[i], expected);
            EXPECT_EQ(values[i]->GetArena(), arena.get());
            EXPECT_EQ(UEMeta::DeclDb::queryDecl(variableId(names[i])), nullptr);
        }
        const auto before = outputFiles();
        UEMeta::DeclDb::serializeIfNeeded(records[1]);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, GlobalAnonymousUnionEmbedsDeclaratorOwnedTypesAndUsesGlobalScope) {
        const auto records = parse("static union { struct { int x; } value; enum { A = 1 } mode; };");
        ASSERT_EQ(records.size(), 2u);
        const auto ir = RecordDeclWrapper{records[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        const auto& values = std::get<Variables>(ir);
        ASSERT_EQ(values.size(), 2u);
        expectProto(values[0]->metadata(), metadata("::value", variableId("::value"), 0));
        auto embedded          = anonymousRecord(1, ParserTypes::RECORD_KIND_STRUCT, 4, 4);
        *embedded.add_fields() = field("x", "int", 32, 0);

        ASSERT_TRUE(values[0]->type_ref().has_anon_record());
        expectProto(values[0]->type_ref().anon_record(), embedded);
        expectProto(values[1]->metadata(), metadata("::mode", variableId("::mode"), 2));

        ASSERT_TRUE(values[1]->type_ref().has_anon_enum());
        const auto& enumeration = values[1]->type_ref().anon_enum();
        ASSERT_EQ(enumeration.enumerators_size(), 1);
        EXPECT_EQ(enumeration.enumerators(0).name(), "A");
        expectVersioned(enumeration.enumerators(0).value(), "1");
        const auto before = outputFiles();
        UEMeta::DeclDb::serializeIfNeeded(records[1]);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, NonVirtualAndVirtualBasesUseByteOffsetsAndDeclaredAccess) {
        const auto records = parse("struct Left { int l; }; struct Right { int r; }; struct Virtual { int v; }; class Derived : Left, protected "
                                   "Right, public virtual Virtual { public: int own; };");
        ASSERT_EQ(records.size(), 4u);
        for (size_t i = 0; i < 3; ++i)
            ASSERT_NE(serialize(records[i]), nullptr);
        // Windows layout: Left@0, Right@4, vbptr@8, own@16, Virtual@24; alignment 8.
        auto expected          = record("::Derived", 3, ParserTypes::RECORD_KIND_CLASS, 32, 8);
        *expected.add_bases()  = base(reference("::Left", recordId("::Left")), ParserTypes::ACCESS_SPECIFIER_PRIVATE, false, 0);
        *expected.add_bases()  = base(reference("::Right", recordId("::Right")), ParserTypes::ACCESS_SPECIFIER_PROTECTED, false, 4, 1);
        *expected.add_bases()  = base(reference("::Virtual", recordId("::Virtual")), ParserTypes::ACCESS_SPECIFIER_PUBLIC, true, 24, 2);
        *expected.add_fields() = field("own", "int", 32, 128);
        expectProto(*serialize(records[3]), expected);
    }

    TEST_F(RecordDeclWrapperTest, DependentBasesKeepPacksAndUnknownTypesWithoutLayout) {
        const auto records = parse("struct Known {}; template<class T, class... Bases> struct Derived : Known, T, Bases... {};");
        ASSERT_EQ(records.size(), 2u);
        // Known deliberately is not serialized: references must not invent identities.
        auto expected = record("::Derived<T, Bases...>", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt, "<typenametypename...>");
        *expected.mutable_template_details()->add_parameters() = typeParameter("T");
        *expected.mutable_template_details()->add_parameters() = typeParameter("Bases", true);
        auto known                                             = builtin("::Known");
        known.set_is_builtin_or_template(false);
        *expected.add_bases() = base(known, ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, std::nullopt);
        *expected.add_bases() = base(builtin("type-parameter-0-0"), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, std::nullopt, 1);
        *expected.add_bases() = base(builtin("type-parameter-0-1..."), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, std::nullopt, 2);
        expectProto(*serialize(records[1]), expected);
    }

    TEST_F(RecordDeclWrapperTest, SpecializedBaseLayoutUsesTheInstanceWhileItsReferenceUsesThePattern) {
        const auto records = parse("template<class T> struct Base { T value; }; struct Derived : Base<double> { char tail; };");
        ASSERT_EQ(records.size(), 2u);
        ASSERT_NE(serialize(records[0]), nullptr);
        auto expected = record("::Derived", 1, ParserTypes::RECORD_KIND_STRUCT, 16, 8);
        *expected.add_bases() =
            base(reference("::Base<double>", recordId("::Base<T>", "<typename>")), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, 0);
        *expected.add_fields() = field("tail", "char", 8, 64);
        expectProto(*serialize(records[1]), expected);
    }

    TEST_F(RecordDeclWrapperTest, StandardNamespaceReferencesRetainTheirHeaderInsteadOfInventingHashes) {
        const auto records = parse("namespace std { struct External {}; } struct Owner { std::External* pointer; };");
        ASSERT_EQ(records.size(), 2u);
        auto expected = record("::Owner", 0, ParserTypes::RECORD_KIND_STRUCT, 8, 8);
        auto pointer  = field("pointer", "::std::External *", 64, 0);
        pointer.mutable_type_ref()->mutable_type_ref()->clear_is_builtin_or_template();
        *pointer.mutable_type_ref()->mutable_type_ref()->mutable_header() = versioned<ParserTypes::VersionedString>("wrapper_fixture.cpp");
        *expected.add_fields() = pointer;
        expectProto(*serialize(records[1]), expected);
    }

    TEST_F(RecordDeclWrapperTest, LambdaFieldUsesAReferenceRatherThanEmbeddingAnUnownedAnonymousRecord) {
        const auto records = parse("struct Owner { decltype([]{}) callback; };");
        ASSERT_EQ(records.size(), 1u);
        auto expected = record("::Owner", 0, ParserTypes::RECORD_KIND_STRUCT, 1, 1);
        auto callback = field("callback", "::Owner::(lambda at wrapper_fixture.cpp:1:25)", 8, 0);
        *callback.mutable_type_ref()->mutable_type_ref()->mutable_is_builtin_or_template() = boolean(false);
        *expected.add_fields() = callback;
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, DependentFieldsKeepKnownBitWidthsAndExpressionsButNoOffsetsOrLayout) {
        const auto records = parse(R"cpp(template<class T, int N> struct Dependent {
            T value = T{};
            unsigned known : 3;
            unsigned dependent : N;
            union { int raised; T other; };
            struct { T retained; } embedded;
            enum { Count = N + 1 };
        };)cpp");
        ASSERT_EQ(records.size(), 3u);
        auto expected = record("::Dependent<T, N>", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt, "<typenameint>");
        *expected.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            parameters { kind: TEMPLATE_PARAMETER_KIND_CLASS type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true } }
            parameters { kind: TEMPLATE_PARAMETER_KIND_NON_TYPE name { versions { source_versions: "test-version" value: "N" } } type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true } }
        )pb");
        auto value                           = field("value", "type-parameter-0-0", std::nullopt, std::nullopt);
        *value.mutable_default_value()       = versioned<ParserTypes::VersionedString>("T{}");
        *expected.add_fields()               = value;
        auto bits                            = field("known", "unsigned int", 3, std::nullopt, 1);
        *bits.mutable_is_bitfield()          = boolean(true);
        *expected.add_fields()               = bits;
        bits.set_name("dependent");
        bits.clear_bit_width();
        *bits.mutable_local_occurrence_index() = versioned<ParserTypes::VersionedUint64>(2);
        *expected.add_fields() = bits;
        *expected.add_fields() = field("raised", "int", std::nullopt, std::nullopt, 3);
        *expected.add_fields() = field("other", "type-parameter-0-0", std::nullopt, std::nullopt, 4);
        auto embedded          = anonymousRecord(1, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt);
        *embedded.add_fields() = field("retained", "type-parameter-0-0", std::nullopt, std::nullopt);
        value                  = field("embedded", "", std::nullopt, std::nullopt, 5);
        value.mutable_type_ref()->clear_type_ref();
        *value.mutable_type_ref()->mutable_anon_record() = embedded;
        *expected.add_fields()                                                                 = value;
        auto constant = staticField("Count", "int", std::nullopt, ParserTypes::VAR_STORAGE_CLASS_STATIC, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        constant.set_is_anon_enum_value(true);
        *constant.mutable_default_value() = versioned<ParserTypes::VersionedString>("N + 1");
        *expected.add_fields()            = constant;
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, PrimaryPartialAndExplicitSpecializationsHaveDistinctIdentitiesAndDetails) {
        const auto records = parse("template<class T> struct Box {}; template<class T> struct Box<T*> {}; template<> struct Box<int> {};");
        ASSERT_EQ(records.size(), 3u);
        const auto primary_id = recordId("::Box<T>", "<typename>");
        auto       primary    = record("::Box<T>", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt, "<typename>");
        *primary.mutable_template_details()->add_parameters() = typeParameter("T");
        expectProto(*serialize(records[0]), primary);
        auto  partial = record("::Box<T *>", 1, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt, "<typename><typename>");
        auto* details = partial.mutable_template_details();
        details->set_specialization_kind(ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
        *details->add_parameters()                   = typeParameter("T");
        *details->mutable_primary_template_decl_id() = reference("Box", primary_id);
        auto* parameter                              = details->add_specialized_parameters();
        parameter->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_GENERIC);
        *parameter->mutable_type() = builtin("type-parameter-0-0 *");
        expectProto(*serialize(records[1]), partial);
        auto specialized = record("::Box<int>", 2, ParserTypes::RECORD_KIND_STRUCT, 1, 1, "<int>");
        details          = specialized.mutable_template_details();
        details->set_specialization_kind(ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
        *details->mutable_primary_template_decl_id() = reference("Box", primary_id);
        parameter                                    = details->add_specialized_parameters();
        parameter->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
        *parameter->mutable_type() = builtin("int");
        expectProto(*serialize(records[2]), specialized);
    }

    TEST_F(RecordDeclWrapperTest, SpecializationOfAnUndefinedPrimaryDoesNotInventItsIdentity) {
        const auto records = parse("template<class T> struct Box; template<> struct Box<int> {};");
        ASSERT_EQ(records.size(), 2u);
        auto  expected = record("::Box<int>", 0, ParserTypes::RECORD_KIND_STRUCT, 1, 1, "<int>");
        auto* details  = expected.mutable_template_details();
        details->set_specialization_kind(ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
        auto* argument = details->add_specialized_parameters();
        argument->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
        *argument->mutable_type() = builtin("int");
        expectProto(*serialize(records[1]), expected);
    }

    TEST_F(RecordDeclWrapperTest, InstantiationsReferenceTheSelectedPrimaryOrPartialPattern) {
        const auto records = parse(R"cpp(
            template<class T> struct Box { T value; };
            template<class T> struct Box<T*> { T* value; };
            template struct Box<int>;
            extern template struct Box<long>;
            Box<double> ordinary;
            Box<int*> partial;
        )cpp");
        ASSERT_GE(records.size(), 2u);
        auto* primary = llvm::cast<clang::CXXRecordDecl>(records[0])->getDescribedClassTemplate();
        ASSERT_NE(primary, nullptr);
        ASSERT_NE(serialize(records[0]), nullptr);
        ASSERT_NE(serialize(records[1]), nullptr);
        const auto primary_id = recordId("::Box<T>", "<typename>");
        const auto partial_id = recordId("::Box<T *>", "<typename><typename>");
        unsigned   count      = 0;
        for (auto* specialization : primary->specializations()) {
            const auto argument         = specialization->getTemplateArgs().get(0).getAsType();
            const bool from_partial     = argument->isPointerType();
            const auto kind             = specialization->getSpecializationKind();
            const bool implicit         = kind == clang::TSK_ImplicitInstantiation;
            const bool declaration_only = kind == clang::TSK_ExplicitInstantiationDeclaration;
            ASSERT_TRUE(implicit || declaration_only || kind == clang::TSK_ExplicitInstantiationDefinition);
            const std::string type = from_partial ? "int *" : implicit ? "double" : declaration_only ? "long" : "int";
            SCOPED_TRACE(type);
            const int64_t size     = from_partial || type == "double" ? 8 : 4;
            auto          expected = record("::Box<" + type + ">", 2 + count, ParserTypes::RECORD_KIND_STRUCT, size, size, "<" + type + ">");
            *expected.add_fields() = field("value", type, size * 8, 0);
            auto* details          = expected.mutable_template_details();
            details->set_specialization_kind(implicit           ? ParserTypes::TEMPLATE_SPECIALIZATION_IMPLICIT
                                             : declaration_only ? ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION
                                                                : ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
            *details->mutable_primary_template_decl_id() = reference("Box", from_partial ? partial_id : primary_id);
            auto* parameter                              = details->add_specialized_parameters();
            parameter->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
            *parameter->mutable_type() = builtin(type);
            expectProto(*serialize(specialization), expected);
            // Registering an instantiation must not redirect type references away from its source pattern.
            const auto query = UEMeta::DeclDb::queryType(specialization->getASTContext().getCanonicalTagType(specialization));
            ASSERT_TRUE(std::holds_alternative<UEMeta::Hash>(query));
            EXPECT_EQ(std::get<UEMeta::Hash>(query), from_partial ? partial_id : primary_id);
            ++count;
        }
        EXPECT_EQ(count, 4u);
    }

    TEST_F(RecordDeclWrapperTest, NumericTemplateArgumentsAndDefaultsAreCorrectAndAffectIdentity) {
        const auto records = parse("template<int N = 2> struct Count {}; template<> struct Count<3> {}; template<> struct Count<4> {};");
        ASSERT_EQ(records.size(), 3u);
        auto expected                        = record("::Count<N>", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt, "<int>");
        *expected.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_NON_TYPE
                name { versions { source_versions: "test-version" value: "N" } }
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
                value { versions { source_versions: "test-version" value: "2" } }
            }
        )pb");
        expectProto(*serialize(records[0]), expected);
        for (size_t i = 1; i < records.size(); ++i) {
            const auto value = std::to_string(i + 2);
            expected         = record("::Count<" + value + ">", i, ParserTypes::RECORD_KIND_STRUCT, 1, 1, "<" + value + ">");
            auto* details    = expected.mutable_template_details();
            details->set_specialization_kind(ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
            *details->mutable_primary_template_decl_id() = reference("Count", recordId("::Count<N>", "<int>"));
            auto* argument                               = details->add_specialized_parameters();
            argument->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE);
            *argument->mutable_value() = versioned<ParserTypes::VersionedString>(value);
            expectProto(*serialize(records[i]), expected);
        }
    }

    TEST_F(RecordDeclWrapperTest, MethodsKeepOwnershipAndVirtualLayoutWhileImplicitNonVirtualMethodsAreOmitted) {
        const auto records = parse(R"cpp(
            struct Base { virtual ~Base(); virtual int read() const = 0; };
            struct Derived : Base { int read() const override; static void create(); void erased() = delete; };
            Derived object;
        )cpp");
        ASSERT_EQ(records.size(), 2u);
        ASSERT_NE(serialize(records[0]), nullptr);
        const auto  before = outputFiles();
        const auto* actual = serialize(records[1]);
        ASSERT_NE(actual, nullptr);
        expectProto(actual->metadata(), metadata("::Derived", recordId("::Derived"), 1));
        EXPECT_EQ(actual->kind(), ParserTypes::RECORD_KIND_STRUCT);
        expectVersioned(actual->size_bytes(), 8);
        expectVersioned(actual->align_bytes(), 8);
        EXPECT_EQ(actual->fields_size(), 0);
        EXPECT_FALSE(actual->has_nested_hashes());
        EXPECT_FALSE(actual->has_template_details());
        ASSERT_EQ(actual->bases_size(), 1);
        expectProto(actual->bases(0), base(reference("::Base", recordId("::Base")), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, 0));
        ASSERT_EQ(actual->methods_size(), 4);
        const std::vector<std::string> names{"read", "create", "erased", "~Derived"};
        for (int i = 0; i < actual->methods_size(); ++i) {
            SCOPED_TRACE(names[i]);
            const auto& method = actual->methods(i);
            EXPECT_EQ(method.name(), names[i]);
            expectId(method.func_id(), recordId("::Derived::" + names[i], i == 0 ? " const" : ""));
            EXPECT_EQ(method.GetArena(), arena.get());
            expectProto(method.is_const(), boolean(i == 0));
            expectProto(method.is_volatile(), boolean(false));
            expectProto(method.is_deleted(), boolean(i == 2));
            expectVersioned(method.access(), ParserTypes::ACCESS_SPECIFIER_PUBLIC);
            expectVersioned(method.virtuality(), i == 0 || i == 3 ? ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL : ParserTypes::FUNCTION_VIRTUALITY_NONE);
            EXPECT_EQ(method.common().kind(), i == 3   ? ParserTypes::FUNCTION_KIND_DESTRUCTOR
                                              : i == 1 ? ParserTypes::FUNCTION_KIND_STATIC_MEMBER
                                                       : ParserTypes::FUNCTION_KIND_MEMBER);
            expectVersioned(method.common().definition_kind(), i == 3   ? ParserTypes::FUNCTION_DEFINITION_DEFAULTED
                                                              : i == 2 ? ParserTypes::FUNCTION_DEFINITION_DELETED
                                                                       : ParserTypes::FUNCTION_DEFINITION_NORMAL);
            expectVersioned(method.common().storage_class(),
                            i == 1 ? ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC : ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED);
            expectVersioned(method.common().consteval_kind(), ParserTypes::CONSTANT_EVALUATION_NONE);
            EXPECT_EQ(method.common().parameters_size(), 0);
            EXPECT_FALSE(method.common().has_template_details());
            EXPECT_FALSE(method.common().has_is_friend());
            EXPECT_FALSE(method.common().has_is_explicit());
            if (i == 3)
                EXPECT_FALSE(method.common().has_return_type());
            else {
                ASSERT_TRUE(method.common().has_return_type());
                expectProto(method.common().return_type(), versionedRef(builtin(i == 0 ? "int" : "void")));
            }
            if (i == 0 || i == 3) {
                expectVersioned(method.vtable_offset(), 0);
                expectVersioned(method.vtable_index(), i == 0 ? 1 : 0);
            }
            else {
                EXPECT_FALSE(method.has_vtable_offset());
                EXPECT_FALSE(method.has_vtable_index());
            }
            EXPECT_EQ(UEMeta::DeclDb::queryDecl(recordId("::Derived::" + names[i], i == 0 ? " const" : "")), nullptr);
        }
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(RecordDeclWrapperTest, DependentVirtualMethodsDoNotRequireAVtableLayout) {
        const auto records = parse("template<class T> struct Abstract { virtual T get() const = 0; };");
        ASSERT_EQ(records.size(), 1u);
        const auto* actual = serialize(records[0]);
        ASSERT_NE(actual, nullptr);
        EXPECT_FALSE(actual->has_size_bytes());
        EXPECT_FALSE(actual->has_align_bytes());
        ASSERT_EQ(actual->methods_size(), 1);
        const auto& method = actual->methods(0);
        EXPECT_EQ(method.name(), "get");
        expectId(method.func_id(), recordId("::Abstract<T>::get", " const"));
        expectVersioned(method.virtuality(), ParserTypes::FUNCTION_VIRTUALITY_PURE);
        expectProto(method.is_const(), boolean(true));
        ASSERT_TRUE(method.common().has_return_type());
        expectProto(method.common().return_type(), versionedRef(builtin("T")));
        EXPECT_FALSE(method.has_vtable_index());
        EXPECT_FALSE(method.has_vtable_offset());
    }

    TEST_F(RecordDeclWrapperTest, UnsetMemberAccessUsesTheOwningRecordDefault) {
        const auto records = parse("class Class { int value; }; struct Struct { int value; };");
        ASSERT_EQ(records.size(), 2u);
        for (size_t i = 0; i < records.size(); ++i) {
            (*records[i]->field_begin())->setAccess(clang::AS_none);
            auto expected =
                record(i == 0 ? "::Class" : "::Struct", i, i == 0 ? ParserTypes::RECORD_KIND_CLASS : ParserTypes::RECORD_KIND_STRUCT, 4, 4);
            *expected.add_fields() =
                field("value", "int", 32, 0, 0, i == 0 ? ParserTypes::ACCESS_SPECIFIER_PRIVATE : ParserTypes::ACCESS_SPECIFIER_PUBLIC);
            expectProto(*serialize(records[i]), expected);
        }
    }

    TEST_F(RecordDeclWrapperTest, MemberTemplatesAreDispatchedAndAliasesFriendsAndImplicitTemplatesAreSkipped) {
        const auto records = parse(R"cpp(struct Owner {
            template<class T> struct Nested {};
            template<class T> using Alias = T;
            template<class T> static int value;
            template<class T> void method(T input);
            template<class T> void ignored(T);
            friend void external();
            static_assert(true);
        };)cpp");
        ASSERT_EQ(records.size(), 2u);
        for (auto* member : records[0]->decls()) {
            if (auto* templated = llvm::dyn_cast<clang::FunctionTemplateDecl>(member); templated && templated->getName() == "ignored")
                templated->setImplicit();
        }
        const auto* actual = serialize(records[0]);
        ASSERT_NE(actual, nullptr);
        expectProto(actual->metadata(), metadata("::Owner", recordId("::Owner"), 0));
        expectVersioned(actual->size_bytes(), 1);
        expectVersioned(actual->align_bytes(), 1);
        ASSERT_EQ(actual->fields_size(), 1);
        expectProto(actual->fields(0), staticField("value", "int", 32));
        ASSERT_EQ(actual->methods_size(), 1);
        const auto& method = actual->methods(0);
        EXPECT_EQ(method.name(), "method");
        expectId(method.func_id(), recordId("::Owner::method", "T<typename>"));
        ASSERT_EQ(method.common().parameters_size(), 1);
        expectVersioned(method.common().parameters(0).name(), "input");
        expectProto(method.common().parameters(0).type_ref(), builtin("T"));
        ParserTypes::TemplateDetails details;
        *details.add_parameters() = typeParameter("T");
        expectProto(method.common().template_details(), details);
        EXPECT_EQ(method.GetArena(), arena.get());
        auto expected = record("::Owner", 0, ParserTypes::RECORD_KIND_STRUCT, 1, 1);
        nestedHash(expected, recordId("::Owner::Nested<T>", "<typename>"));
        expectProto(actual->nested_hashes(), expected.nested_hashes());
        EXPECT_EQ(actual->bases_size(), 0);
        EXPECT_FALSE(actual->has_template_details());
    }

    TEST_F(RecordDeclWrapperTest, CRecordsUsePublicAccessAndFlexibleArraysHaveNoInventedWidth) {
        const auto records = parse("struct Packet { int length; unsigned char bytes[]; };", {"-x", "c", "-std=c17"});
        ASSERT_EQ(records.size(), 1u);
        ASSERT_FALSE(llvm::isa<clang::CXXRecordDecl>(records[0]));
        auto expected          = record("::Packet", 0, ParserTypes::RECORD_KIND_STRUCT, 4, 4);
        *expected.add_fields() = field("length", "int", 32, 0);
        *expected.add_fields() = field("bytes", "unsigned char[]", std::nullopt, 32, 1);
        expectProto(*serialize(records[0]), expected);
    }

    TEST_F(RecordDeclWrapperTest, IncompleteAndInvalidRecordsOmitLayoutInsteadOfReportingZero) {
        // The public entry point normally receives definitions. C records also permit an
        // incomplete node here without CXXRecordDecl::bases() requiring definition data.
        const auto records = parse("struct Forward; struct Invalid { int member; };", {"-x", "c", "-std=c17"});
        ASSERT_EQ(records.size(), 2u);
        const auto forward = record("::Forward", 0, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt);
        expectProto(*serialize(records[0]), forward);
        // A recoverable AST can retain a definition after diagnostics marked it invalid.
        records[1]->setInvalidDecl();
        auto expected          = record("::Invalid", 1, ParserTypes::RECORD_KIND_STRUCT, std::nullopt, std::nullopt);
        *expected.add_fields() = field("member", "int", std::nullopt, std::nullopt);
        expectProto(*serialize(records[1]), expected);
    }

    TEST_F(RecordDeclWrapperTest, NullRecordsAndStandaloneNestedAnonymousStorageAreRejected) {
        const RecordDeclWrapper invalid{nullptr, arena};
        EXPECT_THROW((void)invalid.toIntermediateRepresentation(), RecordError);
        const auto records = parse("struct Outer { union { int x; }; };");
        ASSERT_EQ(records.size(), 2u);
        const RecordDeclWrapper nested{records[1], arena};
        EXPECT_THROW((void)nested.toIntermediateRepresentation(), RecordError);
    }

    TEST_F(RecordDeclWrapperTest, MissingFieldTypeIsRejectedBeforeLayoutOrTypeQueries) {
        // Static members reach putFieldType directly; clearing a non-static field's type
        // would violate Clang's isAnonymousStructOrUnion() precondition during dispatch.
        const auto records = parse("template<class T> struct Broken { static T field; };");
        ASSERT_EQ(records.size(), 1u);
        clang::VarDecl* member = nullptr;
        for (auto* declaration : records[0]->decls())
            if (auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
                member = variable;
        ASSERT_NE(member, nullptr);
        member->setType(clang::QualType{});
        EXPECT_THROW((void)serialize(records[0]), RecordError);
    }

    TEST_F(RecordDeclWrapperTest, ToFileWritesRecordsAndUnionVariablesWithoutReadingThemBack) {
        const auto records = parse("struct StaticWrite {}; struct MemberWrite {}; static union { int x; double y; };");
        ASSERT_EQ(records.size(), 3u);
        auto       ir   = RecordDeclWrapper{records[0], arena}.toIntermediateRepresentation();
        const auto path = outputPath(std::get<Record*>(ir)->metadata(), "recordbin");
        ASSERT_FALSE(std::filesystem::exists(path));
        RecordDeclWrapper::toFile(std::move(ir), arena);
        expectOutput(path);
        const auto before = outputFiles();
        RecordDeclWrapper{records[1], arena}.toFile();
        const auto added = addedFiles(before);
        ASSERT_EQ(added.size(), 1u);
        EXPECT_EQ(added[0].extension(), ".recordbin");
        expectOutput(added[0]);
        ir = RecordDeclWrapper{records[2], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        ASSERT_EQ(std::get<Variables>(ir).size(), 2u);
        std::vector<std::filesystem::path> paths;
        for (const auto* item : std::get<Variables>(ir))
            paths.push_back(outputPath(item->metadata(), "varbin"));
        RecordDeclWrapper::toFile(std::move(ir), arena);
        for (const auto& variable_path : paths)
            expectOutput(variable_path);
    }

    TEST_F(RecordDeclWrapperTest, ToFileHandlesEmptyVectorsAndRejectsMissingInputsAndOpenFailures) {
        const auto before = outputFiles();
        RecordDeclWrapper::toFile(Variables{}, arena);
        EXPECT_EQ(outputFiles(), before);
        EXPECT_THROW(RecordDeclWrapper::toFile(static_cast<Record*>(nullptr), arena), std::invalid_argument);
        EXPECT_THROW(RecordDeclWrapper::toFile(Variables{nullptr}, arena), std::invalid_argument);
        const auto records = parse("struct CannotWrite {};");
        ASSERT_EQ(records.size(), 1u);
        auto* message = serialize(records[0]);
        EXPECT_THROW(RecordDeclWrapper::toFile(message, nullptr), std::invalid_argument);
        const auto path = outputPath(message->metadata(), "recordbin");
        ASSERT_TRUE(std::filesystem::create_directory(path));
        EXPECT_THROW(RecordDeclWrapper::toFile(message, arena), std::runtime_error);
        EXPECT_TRUE(std::filesystem::remove(path));
    }
} // namespace
