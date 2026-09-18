#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>
#include "ProtoAssertions.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "UEMeta/clang/wrappers/EnumDeclWrapper.hpp"
#include "WrapperTest.hpp"

namespace {
    using UEMeta::EnumDeclWrapper;
    using UEMeta::Testing::enumId;
    using UEMeta::Testing::expectBuiltinType;
    using UEMeta::Testing::expectFalse;
    using UEMeta::Testing::expectId;
    using UEMeta::Testing::expectMetadata;
    using UEMeta::Testing::expectProto;
    using UEMeta::Testing::expectVersioned;
    using UEMeta::Testing::metadata;
    using UEMeta::Testing::proto;
    using UEMeta::Testing::variableId;
    using EnumMessage = ParserTypes::TLEnumDeclaration;
    using Variables   = std::vector<ParserTypes::TLGlobalVariableDeclaration*>;
    using MissingType = UEMeta::DeclException<clang::EnumDecl>;

    class EnumDeclWrapperTest : public UEMeta::Testing::WrapperTest {
    protected:
        static void collectEnums(clang::DeclContext* context, std::vector<clang::EnumDecl*>& enums) {
            for (auto* declaration : context->decls()) {
                if (auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration))
                    enums.push_back(enumeration);
                else if (auto* class_template = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                    collectEnums(class_template->getTemplatedDecl(), enums);
                else if (auto* nested = llvm::dyn_cast<clang::DeclContext>(declaration))
                    collectEnums(nested, enums);
            }
        }

        std::vector<clang::EnumDecl*> parse(std::string_view code, std::string_view source_file = "wrapper_fixture.cpp") {
            std::vector<clang::EnumDecl*> enums;
            if (auto* context = parseCode(code, source_file))
                collectEnums(context->getTranslationUnitDecl(), enums);
            return enums;
        }

        EnumMessage* asEnum(clang::EnumDecl* declaration) const {
            auto ir = EnumDeclWrapper{declaration, arena}.toIntermediateRepresentation();
            if (const auto* result = std::get_if<EnumMessage*>(&ir))
                return *result;
            ADD_FAILURE() << "Expected an enum representation";
            return nullptr;
        }
    };

    TEST_F(EnumDeclWrapperTest, NamedEnumPreservesMetadataEnumeratorNamesValuesAndDocumentation) {
        const auto enums = parse(R"cpp(namespace Example {
            /// Enum documentation.
            enum class State : long long {
                Negative = -9223372036854775807LL - 1, ///< First enumerator.
                Zero = 0,
                Next,
                Alias = Next,
                Maximum = 9223372036854775807LL
            };
        })cpp");
        ASSERT_EQ(enums.size(), 1u);
        const auto  occurrence = UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() + 1;
        const auto* message    = asEnum(enums[0]);
        ASSERT_NE(message, nullptr);
        EXPECT_EQ(message->GetArena(), arena.get());
        expectMetadata(message->metadata(), "::Example::State");
        expectVersioned(message->metadata().documentation(), "/// Enum documentation.");
        expectVersioned(message->underlying_type(), "long long");
        ASSERT_TRUE(message->has_scope());
        EXPECT_EQ(message->scope(), ParserTypes::ENUM_SCOPE_CLASS);
        ASSERT_EQ(message->enumerators_size(), 5);
        const std::vector<std::string> names{"Negative", "Zero", "Next", "Alias", "Maximum"};
        const std::vector<std::string> values{"-9223372036854775808", "0", "1", "1", "9223372036854775807"};
        for (int index = 0; index < message->enumerators_size(); ++index) {
            SCOPED_TRACE(index);
            EXPECT_EQ(message->enumerators(index).name(), names[index]);
            expectVersioned(message->enumerators(index).value(), values[index]);
            EXPECT_EQ(message->enumerators(index).has_documentation(), index == 0);
        }
        expectVersioned(message->enumerators(0).documentation(), "///< First enumerator.");
        const auto identity = UEMeta::DeclDb::queryDeclIdentity(enums[0]);
        ASSERT_TRUE(std::holds_alternative<UEMeta::Hash>(identity));
        const auto hash = std::get<UEMeta::Hash>(identity);
        EXPECT_EQ(hash.a, message->metadata().decl_id().a());
        EXPECT_EQ(hash.b, message->metadata().decl_id().b());
        EXPECT_EQ(UEMeta::DeclDb::queryDecl(hash), enums[0]);

        auto expected                = proto<EnumMessage>(R"pb(
            underlying_type { versions { source_versions: "test-version" value: "long long" } }
            scope: ENUM_SCOPE_CLASS
            enumerators {
                name: "Negative"
                documentation { versions { source_versions: "test-version" value: "///< First enumerator." } }
                value { versions { source_versions: "test-version" value: "-9223372036854775808" } }
            }
            enumerators { name: "Zero" value { versions { source_versions: "test-version" value: "0" } } }
            enumerators { name: "Next" value { versions { source_versions: "test-version" value: "1" } } }
            enumerators { name: "Alias" value { versions { source_versions: "test-version" value: "1" } } }
            enumerators { name: "Maximum" value { versions { source_versions: "test-version" value: "9223372036854775807" } } }
        )pb");
        *expected.mutable_metadata() = metadata("::Example::State", enumId("::Example::State"), occurrence, "/// Enum documentation.");
        expectProto(*message, expected);
    }

    TEST_F(EnumDeclWrapperTest, DistinguishesUnscopedClassAndStructEnumsIncludingEmptyEnums) {
        const auto enums = parse("enum Plain { First, Second }; enum class Class {}; enum struct Struct : unsigned short { Value = 7 };");
        ASSERT_EQ(enums.size(), 3u);
        const std::vector<ParserTypes::EnumScope> scopes{ParserTypes::ENUM_SCOPE_UNSCOPED, ParserTypes::ENUM_SCOPE_CLASS,
                                                         ParserTypes::ENUM_SCOPE_STRUCT};
        const std::vector<std::string>            types{"int", "int", "unsigned short"};
        const std::vector<std::string>            names{"::Plain", "::Class", "::Struct"};
        const std::vector<int>                    counts{2, 0, 1};
        for (std::size_t index = 0; index < enums.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = asEnum(enums[index]);
            ASSERT_NE(message, nullptr);
            EXPECT_TRUE(message->has_scope());
            EXPECT_EQ(message->scope(), scopes[index]);
            EXPECT_EQ(message->enumerators_size(), counts[index]);
            expectVersioned(message->underlying_type(), types[index]);
            expectMetadata(message->metadata(), names[index]);
            expectId(message->metadata().decl_id(), enumId(names[index]));
            EXPECT_FALSE(message->metadata().has_documentation());
        }
        const auto* plain = asEnum(enums[0]);
        ASSERT_NE(plain, nullptr);
        ASSERT_EQ(plain->enumerators_size(), 2);
        EXPECT_EQ(plain->enumerators(0).name(), "First");
        EXPECT_EQ(plain->enumerators(1).name(), "Second");
        expectVersioned(plain->enumerators(0).value(), "0");
        expectVersioned(plain->enumerators(1).value(), "1");
        EXPECT_FALSE(plain->enumerators(0).has_documentation());
        EXPECT_FALSE(plain->enumerators(1).has_documentation());
        const auto* structure = asEnum(enums[2]);
        ASSERT_NE(structure, nullptr);
        ASSERT_EQ(structure->enumerators_size(), 1);
        EXPECT_EQ(structure->enumerators(0).name(), "Value");
        expectVersioned(structure->enumerators(0).value(), "7");
        EXPECT_FALSE(structure->enumerators(0).has_documentation());
    }

    TEST_F(EnumDeclWrapperTest, PreservesMaximumUnsignedValueAndWrittenUnderlyingAlias) {
        const auto enums = parse("using Word = unsigned long long; enum class Bits : Word { All = 18446744073709551615ULL };");
        ASSERT_EQ(enums.size(), 1u);
        const auto* message = asEnum(enums[0]);
        ASSERT_NE(message, nullptr);
        expectMetadata(message->metadata(), "::Bits");
        expectId(message->metadata().decl_id(), enumId("::Bits"));
        EXPECT_EQ(message->scope(), ParserTypes::ENUM_SCOPE_CLASS);
        expectVersioned(message->underlying_type(), "Word");
        ASSERT_EQ(message->enumerators_size(), 1);
        EXPECT_EQ(message->enumerators(0).name(), "All");
        expectVersioned(message->enumerators(0).value(), "18446744073709551615");
    }

    TEST_F(EnumDeclWrapperTest, TypedefGivesAnAnonymousEnumAnIdentity) {
        const auto enums = parse("namespace N { typedef enum { Left = -3, Right } Direction; }");
        ASSERT_EQ(enums.size(), 1u);
        ASSERT_TRUE(enums[0]->hasNameForLinkage());
        ASSERT_TRUE(enums[0]->getName().empty());
        const auto* message = asEnum(enums[0]);
        ASSERT_NE(message, nullptr);
        expectMetadata(message->metadata(), "::N::Direction");
        expectId(message->metadata().decl_id(), enumId("::N::Direction"));
        expectVersioned(message->underlying_type(), "int");
        EXPECT_EQ(message->scope(), ParserTypes::ENUM_SCOPE_UNSCOPED);
        ASSERT_EQ(message->enumerators_size(), 2);
        EXPECT_EQ(message->enumerators(0).name(), "Left");
        EXPECT_EQ(message->enumerators(1).name(), "Right");
        expectVersioned(message->enumerators(0).value(), "-3");
        expectVersioned(message->enumerators(1).value(), "-2");
    }

    TEST_F(EnumDeclWrapperTest, EmbeddedAnonymousEnumReturnsAnEnum) {
        const auto enums = parse("namespace N { enum { First = 2, Second } variable; }");
        ASSERT_EQ(enums.size(), 1u);
        ASSERT_FALSE(enums[0]->hasNameForLinkage());
        ASSERT_TRUE(enums[0]->isEmbeddedInDeclarator());
        const auto* message = asEnum(enums[0]);
        ASSERT_NE(message, nullptr);
        EXPECT_FALSE(message->metadata().qualified_name().empty());
        EXPECT_TRUE(message->metadata().has_decl_id());
        EXPECT_FALSE(message->metadata().has_is_anonymous());
        expectVersioned(message->metadata().file_path(), "wrapper_fixture.cpp");
        expectVersioned(message->underlying_type(), "int");
        EXPECT_EQ(message->scope(), ParserTypes::ENUM_SCOPE_UNSCOPED);
        ASSERT_EQ(message->enumerators_size(), 2);
        EXPECT_EQ(message->enumerators(0).name(), "First");
        EXPECT_EQ(message->enumerators(1).name(), "Second");
        expectVersioned(message->enumerators(0).value(), "2");
        expectVersioned(message->enumerators(1).value(), "3");
    }

    TEST_F(EnumDeclWrapperTest, IdentityDependsOnQualifiedNameAndNotUnderlyingTypeOrValues) {
        const auto first  = parse("namespace N { enum class Stable : int { Value = 1 }; }");
        const auto second = parse("namespace N { enum class Stable : unsigned long long { Different = 42 }; }");
        const auto other  = parse("namespace Other { enum class Stable : int { Value = 1 }; }");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(second.size(), 1u);
        ASSERT_EQ(other.size(), 1u);
        const auto* a = asEnum(first[0]);
        const auto* b = asEnum(second[0]);
        const auto* c = asEnum(other[0]);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(a->metadata().decl_id().a(), b->metadata().decl_id().a());
        EXPECT_EQ(a->metadata().decl_id().b(), b->metadata().decl_id().b());
        EXPECT_TRUE(a->metadata().decl_id().a() != c->metadata().decl_id().a() || a->metadata().decl_id().b() != c->metadata().decl_id().b());
        EXPECT_LT(a->metadata().occurrence_index().versions(0).value(), b->metadata().occurrence_index().versions(0).value());
    }

    TEST_F(EnumDeclWrapperTest, SourceFileAndDocumentationChangeMetadataButNotIdentity) {
        const auto first  = parse("/// Earlier enum.\nenum class Relocated : short { Value = 1 };", "before.hpp");
        const auto second = parse("/// Later enum.\nenum class Relocated : short { Value = 2 };", "after.hpp");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(second.size(), 1u);
        const auto  occurrence = UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() + 1;
        const auto* earlier    = asEnum(first[0]);
        const auto* later      = asEnum(second[0]);
        ASSERT_NE(earlier, nullptr);
        ASSERT_NE(later, nullptr);
        auto expected                = proto<EnumMessage>(R"pb(
            scope: ENUM_SCOPE_CLASS
            underlying_type { versions { source_versions: "test-version" value: "short" } }
            enumerators { name: "Value" value { versions { source_versions: "test-version" value: "1" } } }
        )pb");
        *expected.mutable_metadata() = metadata("::Relocated", enumId("::Relocated"), occurrence, "/// Earlier enum.", "before.hpp");
        expectProto(*earlier, expected);
        *expected.mutable_metadata() = metadata("::Relocated", enumId("::Relocated"), occurrence + 1, "/// Later enum.", "after.hpp");
        expected.mutable_enumerators(0)->mutable_value()->mutable_versions(0)->set_value("2");
        expectProto(*later, expected);
    }

    TEST_F(EnumDeclWrapperTest, AnonymousEnumeratorsBecomeOrderedStaticConstexprGlobalsWithCanonicalTypes) {
        const auto enums = parse("namespace Outer::Inner { using Word = long long; enum : Word { Negative = -8, Next, Alias = Next }; }");
        ASSERT_EQ(enums.size(), 1u);
        const auto occurrence = UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() + 1;
        auto       ir         = EnumDeclWrapper{enums[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        const auto& variables = std::get<Variables>(ir);
        ASSERT_EQ(variables.size(), 3u);
        const std::vector<std::string> names{"Negative", "Next", "Alias"};
        const std::vector<std::string> values{"-8", "-7", "-7"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto& variable = *variables[index];
            EXPECT_EQ(variable.GetArena(), arena.get());
            expectMetadata(variable.metadata(), "::Outer::Inner::" + names[index]);
            EXPECT_TRUE(variable.is_anon_enum_value());
            EXPECT_FALSE(variable.has_is_anon_union_value());
            EXPECT_FALSE(variable.has_template_details());
            expectVersioned(variable.storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
            expectVersioned(variable.constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
            expectVersioned(variable.default_value(), values[index]);
            expectBuiltinType(variable.type_ref(), "long long");
            auto       expected          = proto<ParserTypes::TLGlobalVariableDeclaration>(R"pb(
                is_anon_enum_value: true
                type_ref { versions { source_versions: "test-version" value { type_ref {
                    type_name { versions { source_versions: "test-version" value: "long long" } }
                    is_builtin_or_template: true
                } } } }
                storage_class { versions { source_versions: "test-version" value: VAR_STORAGE_CLASS_STATIC } }
                constant_evaluation_kind { versions { source_versions: "test-version" value: CONSTANT_EVALUATION_CONSTEXPR } }
                default_value { versions { source_versions: "test-version" value: "" } }
            )pb");
            const auto name              = "::Outer::Inner::" + names[index];
            *expected.mutable_metadata() = metadata(name, variableId(name), occurrence + index);
            expected.mutable_default_value()->mutable_versions(0)->set_value(values[index]);
            expectProto(variable, expected);
            if (index > 0) {
                EXPECT_LT(variables[index - 1]->metadata().occurrence_index().versions(0).value(),
                          variable.metadata().occurrence_index().versions(0).value());
                EXPECT_TRUE(variables[index - 1]->metadata().decl_id().a() != variable.metadata().decl_id().a() ||
                            variables[index - 1]->metadata().decl_id().b() != variable.metadata().decl_id().b());
            }
        }
    }

    TEST_F(EnumDeclWrapperTest, GlobalAnonymousEnumUsesTheGlobalScope) {
        const auto enums = parse("enum { Global = 9 };");
        ASSERT_EQ(enums.size(), 1u);
        auto ir = EnumDeclWrapper{enums[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        const auto& variables = std::get<Variables>(ir);
        ASSERT_EQ(variables.size(), 1u);
        expectMetadata(variables[0]->metadata(), "::Global");
        expectId(variables[0]->metadata().decl_id(), variableId("::Global"));
        expectVersioned(variables[0]->default_value(), "9");
        expectBuiltinType(variables[0]->type_ref(), "int");
        EXPECT_TRUE(variables[0]->is_anon_enum_value());
    }

    TEST_F(EnumDeclWrapperTest, NestedNamedEnumKeepsItsOwningRecordInTheQualifiedName) {
        const auto enums = parse("namespace N { struct Owner { enum struct Mode : char { Off, On }; }; }");
        ASSERT_EQ(enums.size(), 1u);
        const auto* message = asEnum(enums[0]);
        ASSERT_NE(message, nullptr);
        expectMetadata(message->metadata(), "::N::Owner::Mode");
        expectVersioned(message->underlying_type(), "char");
        ASSERT_EQ(message->enumerators_size(), 2);
        EXPECT_EQ(message->enumerators(0).name(), "Off");
        EXPECT_EQ(message->enumerators(1).name(), "On");
        expectVersioned(message->enumerators(0).value(), "0");
        expectVersioned(message->enumerators(1).value(), "1");
    }

    TEST_F(EnumDeclWrapperTest, AnonymousEnumeratorIdentityIsStableAcrossTypeAndValueChanges) {
        const auto first  = parse("namespace N { enum : int { Stable = -1 }; }");
        const auto second = parse("namespace N { enum : unsigned long long { Stable = 42 }; }");
        const auto other  = parse("namespace Other { enum : int { Stable = -1 }; }");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(second.size(), 1u);
        ASSERT_EQ(other.size(), 1u);
        std::vector<ParserTypes::Hash> identities;
        for (auto* declaration : {first[0], second[0], other[0]}) {
            const auto ir = EnumDeclWrapper{declaration, arena}.toIntermediateRepresentation();
            ASSERT_TRUE(std::holds_alternative<Variables>(ir));
            ASSERT_EQ(std::get<Variables>(ir).size(), 1u);
            identities.push_back(std::get<Variables>(ir)[0]->metadata().decl_id());
        }
        EXPECT_EQ(identities[0].a(), identities[1].a());
        EXPECT_EQ(identities[0].b(), identities[1].b());
        EXPECT_TRUE(identities[0].a() != identities[2].a() || identities[0].b() != identities[2].b());
    }

    TEST_F(EnumDeclWrapperTest, EmptyAnonymousEnumReturnsNoVariablesOrFields) {
        const auto enums = parse("enum {};");
        ASSERT_EQ(enums.size(), 1u);
        auto ir = EnumDeclWrapper{enums[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        EXPECT_TRUE(std::get<Variables>(ir).empty());
        ParserTypes::TLRecordDeclaration record;
        EnumDeclWrapper{enums[0], arena}.serializeAsFields(ParserTypes::ACCESS_SPECIFIER_PUBLIC, &record);
        EXPECT_EQ(record.fields_size(), 0);
    }

    TEST_F(EnumDeclWrapperTest, UsesPromotionTypeWhenIntegerTypeIsUnavailable) {
        const auto enums = parse("enum Named { NamedValue = -1 }; enum { AnonymousValue = -2 };");
        ASSERT_EQ(enums.size(), 2u);
        for (auto* declaration : enums) {
            declaration->setIntegerType(clang::QualType{});
            declaration->setPromotionType(declaration->getASTContext().LongLongTy);
        }
        const auto* named = asEnum(enums[0]);
        ASSERT_NE(named, nullptr);
        expectVersioned(named->underlying_type(), "long long");
        expectMetadata(named->metadata(), "::Named");
        ASSERT_EQ(named->enumerators_size(), 1);
        EXPECT_EQ(named->enumerators(0).name(), "NamedValue");
        expectVersioned(named->enumerators(0).value(), "-1");
        auto ir = EnumDeclWrapper{enums[1], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(ir));
        ASSERT_EQ(std::get<Variables>(ir).size(), 1u);
        expectBuiltinType(std::get<Variables>(ir)[0]->type_ref(), "long long");
        expectMetadata(std::get<Variables>(ir)[0]->metadata(), "::AnonymousValue");
        expectVersioned(std::get<Variables>(ir)[0]->default_value(), "-2");
        ParserTypes::TLRecordDeclaration record;
        EnumDeclWrapper{enums[1], arena}.serializeAsFields(ParserTypes::ACCESS_SPECIFIER_PUBLIC, &record);
        ASSERT_EQ(record.fields_size(), 1);
        expectBuiltinType(record.fields(0).type_ref(), "long long");
        EXPECT_EQ(record.fields(0).name(), "AnonymousValue");
        expectVersioned(record.fields(0).default_value(), "-2");
    }

    TEST_F(EnumDeclWrapperTest, MissingIntegerAndPromotionTypesThrowBeforeProducingOutput) {
        const auto enums = parse("enum Named { Value = 1 };");
        ASSERT_EQ(enums.size(), 1u);
        enums[0]->setIntegerType(clang::QualType{});
        enums[0]->setPromotionType(clang::QualType{});
        const EnumDeclWrapper wrapper{enums[0], arena};
        EXPECT_THROW((void)wrapper.toIntermediateRepresentation(), MissingType);
        EXPECT_THROW(wrapper.toFile(), MissingType);
        ParserTypes::TLRecordDeclaration record;
        record.add_fields()->set_name("existing");
        EXPECT_THROW(wrapper.serializeAsFields(ParserTypes::ACCESS_SPECIFIER_PUBLIC, &record), MissingType);
        ASSERT_EQ(record.fields_size(), 1);
        EXPECT_EQ(record.fields(0).name(), "existing");
    }

    TEST_F(EnumDeclWrapperTest, FieldsAppendInOrderAndPreserveAccessDocumentationFlagsAndValues) {
        const auto enums = parse(R"cpp(struct Owner {
            using Word = short;
            enum : Word {
                First = -4, ///< Field documentation.
                Second,
                Third = 3 + 5
            };
        };)cpp");
        ASSERT_EQ(enums.size(), 1u);
        for (const auto access :
             {ParserTypes::ACCESS_SPECIFIER_PUBLIC, ParserTypes::ACCESS_SPECIFIER_PROTECTED, ParserTypes::ACCESS_SPECIFIER_PRIVATE}) {
            SCOPED_TRACE(access);
            ParserTypes::TLRecordDeclaration record;
            record.add_fields()->set_name("existing");
            EnumDeclWrapper{enums[0], arena}.serializeAsFields(access, &record);
            ASSERT_EQ(record.fields_size(), 4);
            EXPECT_EQ(record.fields(0).name(), "existing");
            const std::vector<std::string> names{"First", "Second", "Third"};
            const std::vector<std::string> values{"-4", "-3", "8"};
            for (int index = 1; index < record.fields_size(); ++index) {
                const auto& field = record.fields(index);
                EXPECT_EQ(field.name(), names[index - 1]);
                expectVersioned(field.access(), access);
                expectVersioned(field.default_value(), values[index - 1]);
                expectVersioned(field.storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
                expectVersioned(field.constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
                expectFalse(field.is_mutable());
                expectFalse(field.is_bitfield());
                expectBuiltinType(field.type_ref(), "short");
                EXPECT_TRUE(field.is_anon_enum_value());
                EXPECT_FALSE(field.has_bit_width());
                EXPECT_FALSE(field.has_offset_bits());
                EXPECT_EQ(field.has_documentation(), index == 1);
                auto expected = proto<ParserTypes::Field>(R"pb(
                    type_ref { versions { source_versions: "test-version" value { type_ref {
                        type_name { versions { source_versions: "test-version" value: "short" } }
                        is_builtin_or_template: true
                    } } } }
                    access { versions { source_versions: "test-version" value: ACCESS_SPECIFIER_PUBLIC } }
                    is_mutable { false_versions: "test-version" }
                    is_bitfield { false_versions: "test-version" }
                    is_anon_enum_value: true
                    storage_class { versions { source_versions: "test-version" value: VAR_STORAGE_CLASS_STATIC } }
                    constant_evaluation_kind { versions { source_versions: "test-version" value: CONSTANT_EVALUATION_CONSTEXPR } }
                    default_value { versions { source_versions: "test-version" value: "" } }
                )pb");
                expected.set_name(names[index - 1]);
                expected.mutable_access()->mutable_versions(0)->set_value(access);
                expected.mutable_default_value()->mutable_versions(0)->set_value(values[index - 1]);
                if (index == 1)
                    *expected.mutable_documentation() = proto<ParserTypes::VersionedString>(R"pb(
                        versions { source_versions: "test-version" value: "///< Field documentation." }
                    )pb");
                expectProto(field, expected);
            }
            expectVersioned(record.fields(1).documentation(), "///< Field documentation.");
        }
    }

    TEST_F(EnumDeclWrapperTest, DependentFieldInitializerRetainsItsExpression) {
        const auto enums = parse("template<int N> struct Owner { enum : int { Dependent = N + 2 }; };");
        ASSERT_EQ(enums.size(), 1u);
        ParserTypes::TLRecordDeclaration record;
        EnumDeclWrapper{enums[0], arena}.serializeAsFields(ParserTypes::ACCESS_SPECIFIER_PUBLIC, &record);
        ASSERT_EQ(record.fields_size(), 1);
        EXPECT_EQ(record.fields(0).name(), "Dependent");
        expectVersioned(record.fields(0).default_value(), "N + 2");
        expectBuiltinType(record.fields(0).type_ref(), "int");
        expectVersioned(record.fields(0).access(), ParserTypes::ACCESS_SPECIFIER_PUBLIC);
        expectVersioned(record.fields(0).storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
        expectVersioned(record.fields(0).constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        expectFalse(record.fields(0).is_mutable());
        expectFalse(record.fields(0).is_bitfield());
        EXPECT_TRUE(record.fields(0).is_anon_enum_value());
        EXPECT_FALSE(record.fields(0).has_bit_width());
        EXPECT_FALSE(record.fields(0).has_offset_bits());
        EXPECT_FALSE(record.fields(0).has_documentation());
    }

    TEST_F(EnumDeclWrapperTest, NamelessEnumeratorDoesNotSetAnOptionalFieldName) {
        const auto enums = parse("struct Owner { enum { Value = 7 }; };");
        ASSERT_EQ(enums.size(), 1u);
        // Exercise the defensive branch using Clang's public AST mutation API.
        (*enums[0]->enumerator_begin())->setDeclName(clang::DeclarationName{});
        ParserTypes::TLRecordDeclaration record;
        EnumDeclWrapper{enums[0], arena}.serializeAsFields(ParserTypes::ACCESS_SPECIFIER_PRIVATE, &record);
        ASSERT_EQ(record.fields_size(), 1);
        EXPECT_FALSE(record.fields(0).has_name());
        expectVersioned(record.fields(0).default_value(), "7");
        expectBuiltinType(record.fields(0).type_ref(), "int");
        expectVersioned(record.fields(0).access(), ParserTypes::ACCESS_SPECIFIER_PRIVATE);
        expectVersioned(record.fields(0).storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
        expectVersioned(record.fields(0).constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        expectFalse(record.fields(0).is_mutable());
        expectFalse(record.fields(0).is_bitfield());
        EXPECT_TRUE(record.fields(0).is_anon_enum_value());
        EXPECT_FALSE(record.fields(0).has_documentation());
        EXPECT_FALSE(record.fields(0).has_bit_width());
        EXPECT_FALSE(record.fields(0).has_offset_bits());
    }

    TEST_F(EnumDeclWrapperTest, StaticToFileWritesBothRepresentationAlternativesWithoutReadingThemBack) {
        const auto enums = parse("enum class Written { Value }; enum { First = 1, Second = 2 };");
        ASSERT_EQ(enums.size(), 2u);
        auto named = EnumDeclWrapper{enums[0], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<EnumMessage*>(named));
        const auto enum_path = outputPath(std::get<EnumMessage*>(named)->metadata(), "enumbin");
        ASSERT_FALSE(std::filesystem::exists(enum_path));
        EnumDeclWrapper::toFile(std::move(named), arena);
        expectOutput(enum_path);

        auto anonymous = EnumDeclWrapper{enums[1], arena}.toIntermediateRepresentation();
        ASSERT_TRUE(std::holds_alternative<Variables>(anonymous));
        std::vector<std::filesystem::path> variable_paths;
        for (const auto* variable : std::get<Variables>(anonymous))
            variable_paths.push_back(outputPath(variable->metadata(), "varbin"));
        ASSERT_EQ(variable_paths.size(), 2u);
        EnumDeclWrapper::toFile(std::move(anonymous), arena);
        for (const auto& path : variable_paths)
            expectOutput(path);
    }

    TEST_F(EnumDeclWrapperTest, MemberToFileWritesNamedAndAnonymousEnums) {
        const auto enums = parse("enum class MemberWritten { Value }; enum { MemberFirst = 3, MemberSecond = 4 };");
        ASSERT_EQ(enums.size(), 2u);
        const auto&                        directory = UEMeta::Config::getConfig().getOutputDirectory().getUnderlyingPath();
        std::vector<std::filesystem::path> before;
        for (const auto& entry : std::filesystem::directory_iterator{directory})
            before.push_back(entry.path());
        for (auto* declaration : enums)
            EnumDeclWrapper{declaration, arena}.toFile();
        int enum_count     = 0;
        int variable_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (std::find(before.begin(), before.end(), entry.path()) != before.end())
                continue;
            expectOutput(entry.path());
            if (entry.path().extension() == ".enumbin")
                ++enum_count;
            else if (entry.path().extension() == ".varbin")
                ++variable_count;
            else
                ADD_FAILURE() << "Unexpected output: " << entry.path();
        }
        EXPECT_EQ(enum_count, 1);
        EXPECT_EQ(variable_count, 2);
    }

    TEST_F(EnumDeclWrapperTest, EmptyToFileIsANoopAndInvalidMessagesOrArenasAreRejected) {
        const auto& directory = UEMeta::Config::getConfig().getOutputDirectory().getUnderlyingPath();
        const auto  before    = std::distance(std::filesystem::directory_iterator{directory}, std::filesystem::directory_iterator{});
        EnumDeclWrapper::toFile(Variables{}, arena);
        EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory}, std::filesystem::directory_iterator{}), before);
        EXPECT_THROW(EnumDeclWrapper::toFile(static_cast<EnumMessage*>(nullptr), arena), std::invalid_argument);
        EXPECT_THROW(EnumDeclWrapper::toFile(Variables{nullptr}, arena), std::invalid_argument);
        const auto enums = parse("enum class NullArena { Value }; enum { NullArenaValue = 2 };");
        ASSERT_EQ(enums.size(), 2u);
        for (auto* declaration : enums) {
            auto ir = EnumDeclWrapper{declaration, arena}.toIntermediateRepresentation();
            EXPECT_THROW(EnumDeclWrapper::toFile(std::move(ir), nullptr), std::invalid_argument);
        }
    }

    TEST_F(EnumDeclWrapperTest, ToFilePropagatesFileOpenFailuresForBothAlternatives) {
        const auto enums = parse("enum class CannotWrite { Value }; enum { CannotWriteValue = 1 };");
        ASSERT_EQ(enums.size(), 2u);
        for (auto* declaration : enums) {
            auto       ir   = EnumDeclWrapper{declaration, arena}.toIntermediateRepresentation();
            const auto path = std::holds_alternative<EnumMessage*>(ir) ? outputPath(std::get<EnumMessage*>(ir)->metadata(), "enumbin")
                                                                       : outputPath(std::get<Variables>(ir).at(0)->metadata(), "varbin");
            // An existing directory cannot be opened as an output file on either platform.
            ASSERT_TRUE(std::filesystem::create_directory(path));
            EXPECT_THROW(EnumDeclWrapper::toFile(std::move(ir), arena), std::runtime_error);
            EXPECT_TRUE(std::filesystem::remove(path));
        }
    }
} // namespace
