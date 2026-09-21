#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "ProtoAssertions.hpp"
#include "UEMeta/clang/wrappers/VarDeclWrapper.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "WrapperTest.hpp"
#include "clang/AST/DeclTemplate.h"

namespace {
    using UEMeta::VarDeclWrapper;
    using UEMeta::Testing::expectBuiltinType;
    using UEMeta::Testing::expectId;
    using UEMeta::Testing::expectMetadata;
    using UEMeta::Testing::expectProto;
    using UEMeta::Testing::expectVersioned;
    using UEMeta::Testing::metadata;
    using UEMeta::Testing::proto;
    using UEMeta::Testing::variableId;
    using VariableMessage = ParserTypes::TLGlobalVariableDeclaration;

    class VarDeclWrapperTest : public UEMeta::Testing::WrapperTest {
    protected:
        static void collectVariables(clang::DeclContext* context, std::vector<clang::VarDecl*>& variables) {
            for (auto* declaration : context->decls()) {
                if (auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
                    variables.push_back(variable);
                else if (auto* variable_template = llvm::dyn_cast<clang::VarTemplateDecl>(declaration))
                    variables.push_back(variable_template->getTemplatedDecl());
                else if (auto* class_template = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                    collectVariables(class_template->getTemplatedDecl(), variables);
                else if (!llvm::isa<clang::FunctionDecl>(declaration)) {
                    if (auto* nested = llvm::dyn_cast<clang::DeclContext>(declaration))
                        collectVariables(nested, variables);
                }
            }
        }

        std::vector<clang::VarDecl*> parse(std::string_view code, std::string_view source_file = "wrapper_fixture.cpp") {
            std::vector<clang::VarDecl*> variables;
            if (auto* context = parseCode(code, source_file))
                collectVariables(context->getTranslationUnitDecl(), variables);
            return variables;
        }

        VariableMessage* serialize(const clang::VarDecl* declaration) const {
            return VarDeclWrapper{declaration, arena}.toIntermediateRepresentation();
        }

        static auto identity(const VariableMessage* message) {
            return std::pair{message->metadata().decl_id().a(), message->metadata().decl_id().b()};
        }

        static VariableMessage builtinVariable(std::string_view name, std::string_view type, uint64_t occurrence,
                                               ParserTypes::VariableStorageClass   storage    = ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                               ParserTypes::ConstantEvaluationKind evaluation = ParserTypes::CONSTANT_EVALUATION_NONE,
                                               std::optional<std::string_view> initializer = std::nullopt, std::string_view documentation = "") {
            auto expected                = proto<VariableMessage>(R"pb(
                type_ref { versions { source_versions: "test-version" value { type_ref {
                    type_name { versions { source_versions: "test-version" value: "" } }
                    is_builtin_or_template: true
                } } } }
                storage_class { versions { source_versions: "test-version" value: VAR_STORAGE_CLASS_UNSPECIFIED } }
                constant_evaluation_kind { versions { source_versions: "test-version" value: CONSTANT_EVALUATION_NONE } }
            )pb");
            *expected.mutable_metadata() = metadata(name, variableId(name), occurrence, documentation);
            expected.mutable_type_ref()
                ->mutable_versions(0)
                ->mutable_value()
                ->mutable_type_ref()
                ->mutable_type_name()
                ->mutable_versions(0)
                ->set_value(type);
            expected.mutable_storage_class()->mutable_versions(0)->set_value(storage);
            expected.mutable_constant_evaluation_kind()->mutable_versions(0)->set_value(evaluation);
            if (initializer) {
                auto* value = expected.mutable_default_value()->add_versions();
                value->add_source_versions("test-version");
                value->set_value(*initializer);
            }
            return expected;
        }

        static void expectTypeName(const VariableMessage* message, std::string_view name) {
            ASSERT_EQ(message->type_ref().versions_size(), 1);
            const auto& version = message->type_ref().versions(0);
            ASSERT_EQ(version.source_versions_size(), 1);
            EXPECT_EQ(version.source_versions(0), "test-version");
            ASSERT_TRUE(version.value().has_type_ref());
            expectVersioned(version.value().type_ref().type_name(), name);
        }
    };

    TEST_F(VarDeclWrapperTest, PreservesMetadataDocumentationInitializerAndArena) {
        const auto variables = parse(R"cpp(namespace Outer::Inner {
            /// Variable documentation.
            int answer = 6 * 7;
        })cpp");
        ASSERT_EQ(variables.size(), 1u);
        const auto* message = serialize(variables[0]);
        ASSERT_NE(message, nullptr);
        EXPECT_EQ(message->GetArena(), arena.get());
        expectMetadata(message->metadata(), "::Outer::Inner::answer");
        expectVersioned(message->metadata().documentation(), "/// Variable documentation.");
        expectBuiltinType(message->type_ref(), "int");
        expectVersioned(message->default_value(), "6 * 7");
        expectVersioned(message->storage_class(), ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);
        expectVersioned(message->constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_NONE);
        EXPECT_FALSE(message->has_template_details());
        EXPECT_FALSE(message->has_is_anon_enum_value());
        EXPECT_FALSE(message->has_is_anon_union_value());
        expectProto(*message, builtinVariable("::Outer::Inner::answer", "int", 0, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                              ParserTypes::CONSTANT_EVALUATION_NONE, "6 * 7", "/// Variable documentation."));
    }

    TEST_F(VarDeclWrapperTest, GlobalAndStaticMemberNamesKeepTheirOwningScopes) {
        const auto variables = parse("int global; namespace N { struct Owner { static int member; }; }");
        ASSERT_EQ(variables.size(), 2u);
        const std::vector<std::string> names{"::global", "::N::Owner::member"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = serialize(variables[index]);
            expectMetadata(message->metadata(), names[index]);
            expectBuiltinType(message->type_ref(), "int");
            EXPECT_FALSE(message->metadata().has_documentation());
            EXPECT_FALSE(message->has_default_value());
            expectProto(*message, builtinVariable(names[index], "int", index,
                                                  index == 0 ? ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED : ParserTypes::VAR_STORAGE_CLASS_STATIC));
        }
    }

    TEST_F(VarDeclWrapperTest, DistinguishesStorageClassesAndGivesThreadLocalPrecedence) {
        const auto variables = parse(R"cpp(
            int ordinary;
            static int internal;
            extern int external;
            extern "C" { extern int c_external; int c_definition; }
            thread_local int local;
            static thread_local int local_static;
            extern thread_local int local_external;
            extern "C" { extern thread_local int local_c; }
        )cpp");
        ASSERT_EQ(variables.size(), 9u);
        const std::vector<std::string> names{"::ordinary", "::internal",     "::external",       "::c_external", "::c_definition",
                                             "::local",    "::local_static", "::local_external", "::local_c"};
        const std::vector<ParserTypes::VariableStorageClass> classes{
            ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,  ParserTypes::VAR_STORAGE_CLASS_STATIC,       ParserTypes::VAR_STORAGE_CLASS_EXTERN,
            ParserTypes::VAR_STORAGE_CLASS_EXTERN_C,     ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,  ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL,
            ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL, ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL, ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(variables[index]->getNameAsString());
            const auto* message = serialize(variables[index]);
            expectProto(*message, builtinVariable(names[index], "int", index, classes[index]));
        }
    }

    TEST_F(VarDeclWrapperTest, ConstAndConstinitAreNotConstexprAndInitializersRemainExpressions) {
        const auto variables = parse("constexpr int evaluated = 1 + 2; const int constant = 3; constinit int initialized = 4;");
        ASSERT_EQ(variables.size(), 3u);
        const std::vector<std::string> initializers{"1 + 2", "3", "4"};
        const std::vector<std::string> names{"::evaluated", "::constant", "::initialized"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = serialize(variables[index]);
            expectVersioned(message->constant_evaluation_kind(),
                            index == 0 ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE);
            expectVersioned(message->default_value(), initializers[index]);
            expectBuiltinType(message->type_ref(), index == 2 ? "int" : "const int");
            expectProto(*message, builtinVariable(names[index], index == 2 ? "int" : "const int", index, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                                  index == 0 ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE,
                                                  initializers[index]));
        }
    }

    TEST_F(VarDeclWrapperTest, PreservesAliasesPointersReferencesArraysAndDeducedTypes) {
        const auto variables = parse(R"cpp(namespace N {
            using Word = unsigned long;
            Word word = 1;
            const Word* pointer = nullptr;
            Word& reference = word;
            int array[2] = {1, 2};
            auto deduced = 42LL;
        })cpp");
        ASSERT_EQ(variables.size(), 5u);
        const std::vector<std::string> types{"::N::Word", "const ::N::Word *", "::N::Word &", "int[2]", "long long"};
        const std::vector<std::string> initializers{"1", "nullptr", "word", "{1, 2}", "42LL"};
        const std::vector<std::string> names{"::N::word", "::N::pointer", "::N::reference", "::N::array", "::N::deduced"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = serialize(variables[index]);
            expectBuiltinType(message->type_ref(), types[index]);
            expectVersioned(message->default_value(), initializers[index]);
            expectProto(*message, builtinVariable(names[index], types[index], index, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                                  ParserTypes::CONSTANT_EVALUATION_NONE, initializers[index]));
        }
    }

    TEST_F(VarDeclWrapperTest, OrdinaryIdentityUsesQualifiedNameNotTypeStorageOrInitializer) {
        const auto first       = parse("namespace N { int stable = 1; }");
        const auto changed     = parse("namespace N { static constexpr long long stable = 42; }");
        const auto other_scope = parse("namespace Other { int stable = 1; }");
        const auto other_name  = parse("namespace N { int different = 1; }");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(changed.size(), 1u);
        ASSERT_EQ(other_scope.size(), 1u);
        ASSERT_EQ(other_name.size(), 1u);
        const auto* original = serialize(first[0]);
        const auto* modified = serialize(changed[0]);
        expectId(original->metadata().decl_id(), variableId("::N::stable"));
        expectId(modified->metadata().decl_id(), variableId("::N::stable"));
        EXPECT_EQ(identity(original), identity(modified));
        EXPECT_NE(identity(original), identity(serialize(other_scope[0])));
        EXPECT_NE(identity(original), identity(serialize(other_name[0])));
        expectVersioned(original->metadata().occurrence_index(), 0u);
        expectVersioned(modified->metadata().occurrence_index(), 1u);
    }

    TEST_F(VarDeclWrapperTest, NamedTagsUseReferencesRatherThanEmbeddedDefinitions) {
        const auto variables = parse("namespace N { struct Named { int field; } object; enum Kind { Value } choice; }");
        ASSERT_EQ(variables.size(), 2u);
        const std::vector<std::string> names{"struct ::N::Named", "enum ::N::Kind"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            auto* tag = variables[index]->getType()->getAsTagDecl();
            ASSERT_NE(tag, nullptr);
            UEMeta::Hash known;
            known.a = 123u + index;
            known.b = 456u + index;
            EXPECT_EQ(UEMeta::DeclDb::queryDecl(known), nullptr);
            UEMeta::DeclDb::addDeclIdentity(tag, known);
            const auto* message = serialize(variables[index]);
            ASSERT_NO_FATAL_FAILURE(expectTypeName(message, names[index]));
            const auto& reference = message->type_ref().versions(0).value().type_ref();
            ASSERT_TRUE(reference.has_decl_id());
            EXPECT_EQ(reference.decl_id().a(), known.a);
            EXPECT_EQ(reference.decl_id().b(), known.b);
        }
    }

    TEST_F(VarDeclWrapperTest, UnresolvedAndTypedefNamedRecordsRemainTypeReferences) {
        const auto variables = parse("struct Unknown; extern Unknown* unknown; typedef struct { int field; } Named; Named named;");
        ASSERT_EQ(variables.size(), 2u);
        const std::vector<std::string> names{"::Unknown *", "::Named"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = serialize(variables[index]);
            ASSERT_NO_FATAL_FAILURE(expectTypeName(message, names[index]));
            const auto& reference = message->type_ref().versions(0).value().type_ref();
            EXPECT_EQ(reference.id_case(), ParserTypes::TypeRef::kIsBuiltinOrTemplate);
            EXPECT_FALSE(reference.is_builtin_or_template());
        }
    }

    TEST_F(VarDeclWrapperTest, EmbedsAnonymousRecordsThroughPointersAndArraysWithWindowsLayout) {
        const auto variables =
            parse("struct { long field; } object = {}; union { long field; } *pointer = nullptr; struct { long field; } array[2] = {};");
        ASSERT_EQ(variables.size(), 3u);
        const std::vector<std::string> names{"::object", "::pointer", "::array"};
        for (std::size_t index = 0; index < variables.size(); ++index) {
            SCOPED_TRACE(index);
            const auto* message = serialize(variables[index]);
            ASSERT_EQ(message->type_ref().versions_size(), 1);
            const auto& version = message->type_ref().versions(0);
            ASSERT_EQ(version.source_versions_size(), 1);
            EXPECT_EQ(version.source_versions(0), "test-version");
            ASSERT_TRUE(version.value().has_anon_record());
            const auto& record = version.value().anon_record();
            EXPECT_EQ(record.GetArena(), arena.get());
            EXPECT_TRUE(record.metadata().is_anonymous());
            EXPECT_EQ(record.kind(), index == 1 ? ParserTypes::RECORD_KIND_UNION : ParserTypes::RECORD_KIND_STRUCT);
            // Windows x64 uses a 32-bit long, unlike the Linux x64 ABI.
            expectVersioned(record.size_bytes(), 4u);
            expectVersioned(record.align_bytes(), 4u);
            ASSERT_EQ(record.fields_size(), 1);
            EXPECT_EQ(record.fields(0).name(), "field");
            expectBuiltinType(record.fields(0).type_ref(), "long");
            auto expected_record = proto<ParserTypes::TLRecordDeclaration>(R"pb(
                metadata {
                    is_anonymous: true
                    file_path { versions { source_versions: "test-version" value: "wrapper_fixture.cpp" } }
                    occurrence_index { versions { source_versions: "test-version" value: 0 } }
                }
                kind: RECORD_KIND_STRUCT
                size_bytes { versions { source_versions: "test-version" value: 4 } }
                align_bytes { versions { source_versions: "test-version" value: 4 } }
                fields {
                    name: "field"
                    access { versions { source_versions: "test-version" value: ACCESS_SPECIFIER_PUBLIC } }
                    type_ref { versions { source_versions: "test-version" value { type_ref {
                        type_name { versions { source_versions: "test-version" value: "long" } }
                        is_builtin_or_template: true
                    } } } }
                    bit_width { versions { source_versions: "test-version" value: 32 } }
                    offset_bits { versions { source_versions: "test-version" value: 0 } }
                    is_mutable { false_versions: "test-version" }
                    is_bitfield { false_versions: "test-version" }
                    storage_class { versions { source_versions: "test-version" value: VAR_STORAGE_CLASS_UNSPECIFIED } }
                    constant_evaluation_kind { versions { source_versions: "test-version" value: CONSTANT_EVALUATION_NONE } }
                }
            )pb");
            expected_record.set_kind(index == 1 ? ParserTypes::RECORD_KIND_UNION : ParserTypes::RECORD_KIND_STRUCT);
            // Each variable serializes its embedded record first, then its own metadata.
            expected_record.mutable_metadata()->mutable_occurrence_index()->mutable_versions(0)->set_value(2 * index);
            auto expected = builtinVariable(names[index], "", 2 * index + 1, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                            ParserTypes::CONSTANT_EVALUATION_NONE, index == 1 ? "nullptr" : "{}");
            *expected.mutable_type_ref()->mutable_versions(0)->mutable_value()->mutable_anon_record() = expected_record;
            expectProto(*message, expected);
        }
    }

    TEST_F(VarDeclWrapperTest, ForwardDeclaredTypesRetainTheirForwardOccurrenceThroughAliasesAndPointers) {
        const auto variables = parse("namespace N { struct Forward; using Alias = Forward; Alias* pointer; struct Forward {}; }");
        ASSERT_EQ(variables.size(), 1u);
        clang::QualType underlying;
        (void)UEMeta::DeclDb::queryType(variables[0]->getType(), &underlying);
        ASSERT_FALSE(underlying.isNull());
        ASSERT_NE(underlying->getAsTagDecl(), nullptr);
        auto* definition = underlying->getAsTagDecl()->getDefinition();
        ASSERT_NE(definition, nullptr);
        UEMeta::DeclDb::addForwardDeclaration(definition);
        const auto reference = UEMeta::DeclDb::queryDeclIdentity(definition);
        ASSERT_TRUE(std::holds_alternative<uint64_t>(reference));
        EXPECT_EQ(std::get<uint64_t>(reference), 0u);
        const auto* message = serialize(variables[0]);
        ASSERT_NO_FATAL_FAILURE(expectTypeName(message, "::N::Alias *"));
        const auto& type = message->type_ref().versions(0).value().type_ref();
        ASSERT_TRUE(type.has_forward_decl_index());
        EXPECT_EQ(type.forward_decl_index(), 0u);
        expectVersioned(message->metadata().occurrence_index(), 1u);
    }

    TEST_F(VarDeclWrapperTest, StandaloneAnonymousEnumsAreReferencedRatherThanEmbeddedAgain) {
        const auto variables = parse("enum { First = 1 }; decltype(First) value = First;");
        ASSERT_EQ(variables.size(), 1u);
        auto* enumeration = variables[0]->getType()->getAsTagDecl();
        ASSERT_NE(enumeration, nullptr);
        ASSERT_FALSE(enumeration->hasNameForLinkage());
        ASSERT_FALSE(enumeration->isEmbeddedInDeclarator());
        const auto* message = serialize(variables[0]);
        expectMetadata(message->metadata(), "::value");
        ASSERT_EQ(message->type_ref().versions_size(), 1);
        ASSERT_TRUE(message->type_ref().versions(0).value().has_type_ref());
        const auto& type = message->type_ref().versions(0).value().type_ref();
        EXPECT_EQ(type.id_case(), ParserTypes::TypeRef::kIsBuiltinOrTemplate);
        EXPECT_FALSE(type.is_builtin_or_template());
        expectVersioned(type.type_name(), "decltype(First)");
        expectVersioned(message->default_value(), "First");
    }

    TEST_F(VarDeclWrapperTest, EmbedsAnonymousEnumDefinitionIncludingValues) {
        const auto variables = parse("namespace N { enum : short { First = -2, Second } value; }");
        ASSERT_EQ(variables.size(), 1u);
        const auto* message = serialize(variables[0]);
        expectMetadata(message->metadata(), "::N::value");
        ASSERT_EQ(message->type_ref().versions_size(), 1);
        const auto& version = message->type_ref().versions(0);
        ASSERT_EQ(version.source_versions_size(), 1);
        EXPECT_EQ(version.source_versions(0), "test-version");
        ASSERT_TRUE(version.value().has_anon_enum());
        const auto& enumeration = version.value().anon_enum();
        EXPECT_EQ(enumeration.GetArena(), arena.get());
        EXPECT_TRUE(enumeration.has_scope());
        EXPECT_EQ(enumeration.scope(), ParserTypes::ENUM_SCOPE_UNSCOPED);
        EXPECT_FALSE(enumeration.metadata().has_documentation());
        EXPECT_FALSE(message->has_is_anon_enum_value());
        EXPECT_FALSE(message->has_is_anon_union_value());
        EXPECT_FALSE(message->has_template_details());
        EXPECT_FALSE(message->has_default_value());
        expectVersioned(message->storage_class(), ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);
        expectVersioned(message->constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_NONE);
        expectVersioned(enumeration.underlying_type(), "short");
        ASSERT_EQ(enumeration.enumerators_size(), 2);
        EXPECT_EQ(enumeration.enumerators(0).name(), "First");
        EXPECT_EQ(enumeration.enumerators(1).name(), "Second");
        expectVersioned(enumeration.enumerators(0).value(), "-2");
        expectVersioned(enumeration.enumerators(1).value(), "-1");
        EXPECT_FALSE(enumeration.enumerators(0).has_documentation());
        EXPECT_FALSE(enumeration.enumerators(1).has_documentation());
    }

    TEST_F(VarDeclWrapperTest, PrimaryTemplatePreservesTypeAndOrderedParametersWithDefaults) {
        const auto variables = parse("template<typename T = long, int Count = 3> constexpr T value = Count;");
        ASSERT_EQ(variables.size(), 1u);
        const auto* message = serialize(variables[0]);
        expectMetadata(message->metadata(), "::value");
        expectBuiltinType(message->type_ref(), "const T");
        expectVersioned(message->default_value(), "Count");
        ASSERT_TRUE(message->has_template_details());
        const auto& details = message->template_details();
        EXPECT_EQ(details.specialization_kind(), ParserTypes::TEMPLATE_SPECIALIZATION_NONE);
        EXPECT_FALSE(details.has_primary_template_decl_id());
        EXPECT_EQ(details.specialized_parameters_size(), 0);
        ASSERT_EQ(details.parameters_size(), 2);
        const auto& type = details.parameters(0);
        EXPECT_EQ(type.kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME);
        expectVersioned(type.type().type_name(), "T");
        EXPECT_TRUE(type.type().is_builtin_or_template());
        ASSERT_EQ(type.default_type().versions_size(), 1);
        expectVersioned(type.default_type().versions(0).value().type_name(), "long");
        const auto& count = details.parameters(1);
        EXPECT_EQ(count.kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_NON_TYPE);
        expectVersioned(count.name(), "Count");
        expectVersioned(count.type().type_name(), "int");
        expectVersioned(count.value(), "3");
        auto expected =
            builtinVariable("::value", "const T", 0, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR, "Count");
        *expected.mutable_metadata()         = metadata("::value", variableId("::valueconst T<typenameint>"), 0);
        *expected.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_NONE
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_TYPENAME
                type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true }
                default_type { versions { source_versions: "test-version" value {
                    type_name { versions { source_versions: "test-version" value: "long" } }
                    is_builtin_or_template: true
                } } }
            }
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_NON_TYPE
                name { versions { source_versions: "test-version" value: "Count" } }
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
                value { versions { source_versions: "test-version" value: "3" } }
            }
        )pb");
        expectProto(*message, expected);
    }

    TEST_F(VarDeclWrapperTest, PartialAndExplicitSpecializationsPreserveArgumentsAndPrimaryIdentity) {
        const auto variables = parse(R"cpp(
            template<typename T, int N> constexpr int selected = 0;
            template<typename T> constexpr int selected<T*, 2> = 1;
            template<> constexpr int selected<int, 3> = 2;
        )cpp");
        ASSERT_EQ(variables.size(), 3u);
        ASSERT_NE(variables[0]->getDescribedVarTemplate(), nullptr);
        ASSERT_TRUE(llvm::isa<clang::VarTemplatePartialSpecializationDecl>(variables[1]));
        ASSERT_TRUE(llvm::isa<clang::VarTemplateSpecializationDecl>(variables[2]));
        const auto* primary    = serialize(variables[0]);
        const auto  primary_id = variableId("::selectedconst int<typenameint>");
        expectId(primary->metadata().decl_id(), primary_id);
        UEMeta::DeclDb::addDeclIdentity(variables[0], primary_id);
        const auto* partial       = serialize(variables[1]);
        const auto* explicit_spec = serialize(variables[2]);
        EXPECT_NE(identity(primary), identity(partial));
        EXPECT_NE(identity(primary), identity(explicit_spec));
        EXPECT_NE(identity(partial), identity(explicit_spec));
        for (const auto* message : {partial, explicit_spec}) {
            const auto& details = message->template_details();
            EXPECT_EQ(details.specialization_kind(), ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
            ASSERT_TRUE(details.has_primary_template_decl_id());
            EXPECT_EQ(details.primary_template_decl_id().decl_id().a(), identity(primary).first);
            EXPECT_EQ(details.primary_template_decl_id().decl_id().b(), identity(primary).second);
            expectVersioned(details.primary_template_decl_id().type_name(), "selected");
            ASSERT_EQ(details.specialized_parameters_size(), 2);
            EXPECT_EQ(details.specialized_parameters(1).kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE);
        }
        ASSERT_EQ(partial->template_details().parameters_size(), 1);
        expectVersioned(partial->template_details().parameters(0).type().type_name(), "T");
        EXPECT_EQ(partial->template_details().specialized_parameters(0).kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_GENERIC);
        // Clang's specialization argument list uses canonical template-parameter names.
        expectVersioned(partial->template_details().specialized_parameters(0).type().type_name(), "type-parameter-0-0 *");
        expectVersioned(partial->template_details().specialized_parameters(1).value(), "2");
        EXPECT_EQ(explicit_spec->template_details().parameters_size(), 0);
        expectVersioned(explicit_spec->template_details().specialized_parameters(0).type().type_name(), "int");
        expectVersioned(explicit_spec->template_details().specialized_parameters(1).value(), "3");
        expectVersioned(partial->default_value(), "1");
        expectVersioned(explicit_spec->default_value(), "2");

        auto partial_details = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_TYPENAME
                type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true }
            }
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_GENERIC
                type { type_name { versions { source_versions: "test-version" value: "type-parameter-0-0 *" } } is_builtin_or_template: true }
            }
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE
                value { versions { source_versions: "test-version" value: "2" } }
            }
            primary_template_decl_id { type_name { versions { source_versions: "test-version" value: "selected" } } }
        )pb");
        primary_id.putProtoHash(partial_details.mutable_primary_template_decl_id()->mutable_decl_id());
        auto expected_partial                        = builtinVariable("::selected", "const int", 1, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                                                       ParserTypes::CONSTANT_EVALUATION_CONSTEXPR, "1");
        *expected_partial.mutable_metadata()         = metadata("::selected", variableId("::selectedconst int<typename><typename2>"), 1);
        *expected_partial.mutable_template_details() = partial_details;
        expectProto(*partial, expected_partial);

        auto explicit_details = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE
                value { versions { source_versions: "test-version" value: "3" } }
            }
            primary_template_decl_id { type_name { versions { source_versions: "test-version" value: "selected" } } }
        )pb");
        primary_id.putProtoHash(explicit_details.mutable_primary_template_decl_id()->mutable_decl_id());
        auto expected_explicit                        = builtinVariable("::selected", "const int", 2, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED,
                                                                        ParserTypes::CONSTANT_EVALUATION_CONSTEXPR, "2");
        *expected_explicit.mutable_metadata()         = metadata("::selected", variableId("::selectedconst int<int3>"), 2);
        *expected_explicit.mutable_template_details() = explicit_details;
        expectProto(*explicit_spec, expected_explicit);
    }

    TEST_F(VarDeclWrapperTest, SpecializationWithUnregisteredPrimaryDoesNotInventAnIdentity) {
        const auto variables = parse("template<class T> int unregistered = 0; template<> int unregistered<int> = 1;");
        ASSERT_EQ(variables.size(), 2u);
        const auto* message = serialize(variables[1]);
        ASSERT_TRUE(message->has_template_details());
        EXPECT_FALSE(message->template_details().has_primary_template_decl_id());
        EXPECT_EQ(message->template_details().specialized_parameters_size(), 1);
        expectVersioned(message->default_value(), "1");
        expectProto(message->template_details(), proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
        )pb"));
        expectId(message->metadata().decl_id(), variableId("::unregisteredint<int>"));
    }

    TEST_F(VarDeclWrapperTest, TemplateIdentityIncludesDeclaredTypeAndParameterKindsButNotDefaultsOrNames) {
        const auto first           = parse("template<typename T, int N = 1> int stable = N;");
        const auto renamed         = parse("template<class U, int Count = 5> int stable = Count + 1;");
        const auto other_type      = parse("template<typename T, int N = 1> long stable = N;");
        const auto other_parameter = parse("template<typename T, long N = 1> int stable = N;");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(renamed.size(), 1u);
        ASSERT_EQ(other_type.size(), 1u);
        ASSERT_EQ(other_parameter.size(), 1u);
        const auto original = identity(serialize(first[0]));
        EXPECT_EQ(original, identity(serialize(renamed[0])));
        EXPECT_NE(original, identity(serialize(other_type[0])));
        EXPECT_NE(original, identity(serialize(other_parameter[0])));
    }

    TEST_F(VarDeclWrapperTest, TemplatePacksPreserveParameterAndSpecializedValueOrder) {
        const auto variables = parse("template<int... Values> int packed = 0; template<> int packed<1, 2, 3> = 6;");
        ASSERT_EQ(variables.size(), 2u);
        const auto* primary = serialize(variables[0]);
        ASSERT_EQ(primary->template_details().parameters_size(), 1);
        EXPECT_TRUE(primary->template_details().parameters(0).is_parameter_pack());
        expectProto(primary->template_details(), proto<ParserTypes::TemplateDetails>(R"pb(
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_NON_TYPE
                is_parameter_pack: true
                name { versions { source_versions: "test-version" value: "Values" } }
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
        )pb"));
        expectId(primary->metadata().decl_id(), variableId("::packedint<int...>"));
        const auto* specialization = serialize(variables[1]);
        ASSERT_EQ(specialization->template_details().specialized_parameters_size(), 3);
        for (int index = 0; index < 3; ++index) {
            auto expected = proto<ParserTypes::TemplateParameter>(R"pb(
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE
                value { versions { source_versions: "test-version" value: "" } }
            )pb");
            expected.mutable_value()->mutable_versions(0)->set_value(std::to_string(index + 1));
            expectProto(specialization->template_details().specialized_parameters(index), expected);
        }
        EXPECT_EQ(specialization->template_details().parameters_size(), 0);
        EXPECT_FALSE(specialization->template_details().has_primary_template_decl_id());
        EXPECT_EQ(specialization->template_details().specialization_kind(), ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT);
        expectId(specialization->metadata().decl_id(), variableId("::packedint<123>"));
        expectVersioned(specialization->default_value(), "6");
        EXPECT_NE(identity(primary), identity(specialization));
    }

    TEST_F(VarDeclWrapperTest, SourceFileAndDocumentationChangeMetadataButNotIdentity) {
        const auto first  = parse("/// Earlier variable.\nint relocated = 1;", "before.hpp");
        const auto second = parse("/// Later variable.\nint relocated = 2;", "after.hpp");
        ASSERT_EQ(first.size(), 1u);
        ASSERT_EQ(second.size(), 1u);
        const auto* earlier = serialize(first[0]);
        const auto* later   = serialize(second[0]);
        auto        expected =
            builtinVariable("::relocated", "int", 0, ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED, ParserTypes::CONSTANT_EVALUATION_NONE, "1");
        *expected.mutable_metadata() = metadata("::relocated", variableId("::relocated"), 0, "/// Earlier variable.", "before.hpp");
        expectProto(*earlier, expected);
        *expected.mutable_metadata() = metadata("::relocated", variableId("::relocated"), 1, "/// Later variable.", "after.hpp");
        expected.mutable_default_value()->mutable_versions(0)->set_value("2");
        expectProto(*later, expected);
    }

    TEST_F(VarDeclWrapperTest, TemplateTemplateParametersKeepNestedParametersAndConcreteTemplateArguments) {
        const auto variables = parse(R"cpp(
            template<class T> struct Box {};
            template<template<class> class Container, class... Types> int factory = 0;
            template<> int factory<Box, int, long> = 1;
        )cpp");
        ASSERT_EQ(variables.size(), 2u);
        const auto* primary    = serialize(variables[0]);
        const auto& parameters = primary->template_details().parameters();
        ASSERT_EQ(parameters.size(), 2);
        EXPECT_EQ(parameters[0].kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
        expectVersioned(parameters[0].type().type_name(), "Container");
        ASSERT_EQ(parameters[0].parameters_size(), 1);
        EXPECT_EQ(parameters[0].parameters(0).kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS);
        EXPECT_TRUE(parameters[1].is_parameter_pack());
        expectVersioned(parameters[1].type().type_name(), "Types");
        const auto* specialization = serialize(variables[1]);
        const auto& arguments      = specialization->template_details().specialized_parameters();
        ASSERT_EQ(arguments.size(), 3);
        EXPECT_EQ(arguments[0].kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TEMPLATE);
        expectVersioned(arguments[0].type().type_name(), "::Box");
        EXPECT_EQ(arguments[1].kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
        expectVersioned(arguments[1].type().type_name(), "int");
        EXPECT_EQ(arguments[2].kind(), ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
        expectVersioned(arguments[2].type().type_name(), "long");
        EXPECT_NE(identity(primary), identity(specialization));
    }

    TEST_F(VarDeclWrapperTest, SpecializationIdentityIncludesConcreteArgumentValues) {
        const auto variables = parse("template<int N> int value = N; template<> int value<1> = 7; template<> int value<2> = 7;");
        ASSERT_EQ(variables.size(), 3u);
        const auto* first  = serialize(variables[1]);
        const auto* second = serialize(variables[2]);
        ASSERT_EQ(first->template_details().specialized_parameters_size(), 1);
        ASSERT_EQ(second->template_details().specialized_parameters_size(), 1);
        expectVersioned(first->template_details().specialized_parameters(0).value(), "1");
        expectVersioned(second->template_details().specialized_parameters(0).value(), "2");
        EXPECT_NE(identity(first), identity(second));
    }

    TEST_F(VarDeclWrapperTest, InvalidEmbeddedEnumPropagatesItsErrorWithoutWritingAFile) {
        const auto variables = parse("enum { Value = 1 } invalid;");
        ASSERT_EQ(variables.size(), 1u);
        auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(variables[0]->getType()->getAsTagDecl());
        ASSERT_NE(enumeration, nullptr);
        // Model an incomplete/recovery AST using Clang's public setters.
        enumeration->setIntegerType({});
        enumeration->setPromotionType({});
        const auto&          directory = UEMeta::Config::getConfig().getOutputDirectory().getUnderlyingPath();
        const auto           before    = std::distance(std::filesystem::directory_iterator{directory}, std::filesystem::directory_iterator{});
        const VarDeclWrapper wrapper{variables[0], arena};
        using InvalidEnum = UEMeta::DeclException<clang::EnumDecl>;
        EXPECT_THROW((void)wrapper.toIntermediateRepresentation(), InvalidEnum);
        EXPECT_THROW(wrapper.toFile(), InvalidEnum);
        EXPECT_EQ(std::distance(std::filesystem::directory_iterator{directory}, std::filesystem::directory_iterator{}), before);
    }

    TEST_F(VarDeclWrapperTest, StaticToFileWritesTheProvidedIntermediateRepresentation) {
        const auto variables = parse("int written = 7;");
        ASSERT_EQ(variables.size(), 1u);
        const auto* message = serialize(variables[0]);
        const auto  path    = outputPath(message->metadata(), "varbin");
        ASSERT_FALSE(std::filesystem::exists(path));
        VarDeclWrapper::toFile(message, arena);
        expectOutput(path);
    }

    TEST_F(VarDeclWrapperTest, MemberToFileWritesOneVariableFile) {
        const auto variables = parse("int member_written = 8;");
        ASSERT_EQ(variables.size(), 1u);
        const auto&                        directory = UEMeta::Config::getConfig().getOutputDirectory().getUnderlyingPath();
        std::vector<std::filesystem::path> before;
        for (const auto& entry : std::filesystem::directory_iterator{directory})
            before.push_back(entry.path());
        VarDeclWrapper{variables[0], arena}.toFile();
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (std::find(before.begin(), before.end(), entry.path()) != before.end())
                continue;
            expectOutput(entry.path());
            EXPECT_EQ(entry.path().extension(), ".varbin");
            ++count;
        }
        EXPECT_EQ(count, 1);
    }

    TEST_F(VarDeclWrapperTest, ToFileRejectsMissingMessageOrArena) {
        EXPECT_THROW(VarDeclWrapper::toFile(nullptr, arena), std::invalid_argument);
        const auto variables = parse("int no_arena;");
        ASSERT_EQ(variables.size(), 1u);
        EXPECT_THROW(VarDeclWrapper::toFile(serialize(variables[0]), nullptr), std::invalid_argument);
    }

    TEST_F(VarDeclWrapperTest, ToFilePropagatesFileOpenFailure) {
        const auto variables = parse("int cannot_write;");
        ASSERT_EQ(variables.size(), 1u);
        const auto* message = serialize(variables[0]);
        const auto  path    = outputPath(message->metadata(), "varbin");
        ASSERT_TRUE(std::filesystem::create_directory(path));
        EXPECT_THROW(VarDeclWrapper::toFile(message, arena), std::runtime_error);
        EXPECT_TRUE(std::filesystem::remove(path));
    }
} // namespace
