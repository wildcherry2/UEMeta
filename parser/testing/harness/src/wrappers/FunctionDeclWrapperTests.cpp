#include "CallableTest.hpp"
#include "UEMeta/clang/wrappers/FunctionDeclWrapper.hpp"

namespace {
    using namespace UEMeta::Testing;
    using UEMeta::FunctionDeclWrapper;
    using FunctionMessage = ParserTypes::TLFreeFunctionDeclaration;

    class FunctionDeclWrapperTest : public CallableTest {
    protected:
        FunctionMessage* serialize(const clang::FunctionDecl* declaration) const {
            return FunctionDeclWrapper{declaration, arena}.toIntermediateRepresentation();
        }

        static FunctionMessage expectedFunction(std::string_view name, std::string_view signature, uint64_t occurrence,
                                                const ParserTypes::FunctionCommon& traits, std::string_view documentation = "") {
            FunctionMessage expected;
            *expected.mutable_metadata() = metadata(name, functionId(name, signature), occurrence, documentation);
            *expected.mutable_common()   = traits;
            return expected;
        }
    };

    TEST_F(FunctionDeclWrapperTest, DeclarationPreservesCompleteMetadataOrderedParametersDefaultsAndIdentity) {
        const auto functions = parse(R"cpp(namespace Outer::Inner {
            /// Function documentation.
            long compute(int count, const char* label = "result");
        })cpp");
        ASSERT_EQ(functions.size(), 1u);
        const auto* message = serialize(functions[0]);
        EXPECT_EQ(message->GetArena(), arena.get());
        auto traits              = common(ParserTypes::FUNCTION_KIND_FREE, "long");
        *traits.add_parameters() = parameter("count", "int");
        *traits.add_parameters() = parameter("label", "const char *", "\"result\"");
        expectProto(*message, expectedFunction("::Outer::Inner::compute", "intconst char *", 0, traits, "/// Function documentation."));
        const auto id       = functionId("::Outer::Inner::compute", "intconst char *");
        const auto resolved = UEMeta::DeclDb::queryDeclIdentity(functions[0]);
        ASSERT_TRUE(std::holds_alternative<UEMeta::Hash>(resolved));
        EXPECT_EQ(std::get<UEMeta::Hash>(resolved), id);
        EXPECT_EQ(UEMeta::DeclDb::queryDecl(id), functions[0]);
    }

    TEST_F(FunctionDeclWrapperTest, DistinguishesStorageClassesIncludingCLinkageDeclarations) {
        const auto functions = parse(R"cpp(
            void ordinary(); static void internal(); extern void external();
            extern "C" { extern void c_function(); void c_block(); }
            extern "C" void c_linkage_only();
        )cpp");
        ASSERT_EQ(functions.size(), 6u);
        // Linkage and the written storage class are separate properties in Clang.
        ASSERT_EQ(functions[3]->getStorageClass(), clang::SC_Extern);
        ASSERT_TRUE(functions[3]->isExternC());
        ASSERT_EQ(functions[5]->getStorageClass(), clang::SC_None);
        ASSERT_TRUE(functions[5]->isExternC());
        const std::vector<std::string> names{"::ordinary", "::internal", "::external", "::c_function", "::c_block", "::c_linkage_only"};
        const std::vector<ParserTypes::FunctionStorageClass> storage{
            ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED, ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC,
            ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN,      ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN_C,
            ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED, ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(names[index]);
            expectProto(*serialize(functions[index]),
                        expectedFunction(names[index], "", index, common(ParserTypes::FUNCTION_KIND_FREE, "void", storage[index])));
        }
    }

    TEST_F(FunctionDeclWrapperTest, DefinitionsPreserveBodiesAndConstantEvaluationKinds) {
        const auto functions = parse("int ordinary() { return 1; } constexpr int constant() { return 2; } consteval int immediate() { return 3; }");
        ASSERT_EQ(functions.size(), 3u);
        const std::vector<ParserTypes::ConstantEvaluationKind> kinds{
            ParserTypes::CONSTANT_EVALUATION_NONE, ParserTypes::CONSTANT_EVALUATION_CONSTEXPR, ParserTypes::CONSTANT_EVALUATION_CONSTEVAL};
        const std::vector<std::string> names{"::ordinary", "::constant", "::immediate"};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(names[index]);
            auto traits = common(ParserTypes::FUNCTION_KIND_FREE, "int", ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED, kinds[index]);
            *traits.mutable_inline_definition() = versioned<ParserTypes::VersionedString>("{\n    return " + std::to_string(index + 1) + ";\n}\n");
            expectProto(*serialize(functions[index]), expectedFunction(names[index], "", index, traits));
        }
    }

    TEST_F(FunctionDeclWrapperTest, APrototypeDoesNotStealTheDefinitionsBodyAndDefaultsSurviveRedeclaration) {
        const auto functions = parse("int redeclared(int value = 4); int redeclared(int renamed) { return renamed + 1; }");
        ASSERT_EQ(functions.size(), 2u);
        const auto* prototype                = serialize(functions[0]);
        const auto* definition               = serialize(functions[1]);
        auto        declaration_traits       = common(ParserTypes::FUNCTION_KIND_FREE, "int");
        *declaration_traits.add_parameters() = parameter("value", "int", "4");
        expectProto(prototype->common(), declaration_traits);
        auto definition_traits                         = common(ParserTypes::FUNCTION_KIND_FREE, "int");
        *definition_traits.add_parameters()            = parameter("renamed", "int", "4");
        *definition_traits.mutable_inline_definition() = versioned<ParserTypes::VersionedString>("{\n    return renamed + 1;\n}\n");
        expectProto(definition->common(), definition_traits);
        expectId(prototype->metadata().decl_id(), functionId("::redeclared", "int"));
        expectId(definition->metadata().decl_id(), functionId("::redeclared", "int"));
        const auto reference = UEMeta::DeclDb::queryDeclIdentity(functions[0]);
        ASSERT_TRUE(std::holds_alternative<UEMeta::Hash>(reference));
        EXPECT_EQ(std::get<UEMeta::Hash>(reference), functionId("::redeclared", "int"));
    }

    TEST_F(FunctionDeclWrapperTest, IdentityIgnoresReturnTypeNamesDefaultsStorageAndBodiesButDistinguishesOverloadsAndScopes) {
        const auto first    = parse("namespace N { int stable(int value = 1); }");
        const auto changed  = parse("namespace N { static long stable(int renamed = 9) { return renamed; } }");
        const auto overload = parse("namespace N { int stable(long value); }");
        const auto other    = parse("namespace Other { int stable(int value); }");
        for (const auto* group : {&first, &changed, &overload, &other})
            ASSERT_EQ(group->size(), 1u);
        expectId(serialize(first[0])->metadata().decl_id(), functionId("::N::stable", "int"));
        expectId(serialize(changed[0])->metadata().decl_id(), functionId("::N::stable", "int"));
        expectId(serialize(overload[0])->metadata().decl_id(), functionId("::N::stable", "long"));
        expectId(serialize(other[0])->metadata().decl_id(), functionId("::Other::stable", "int"));
        EXPECT_NE(functionId("::N::stable", "int"), functionId("::N::stable", "long"));
        EXPECT_NE(functionId("::N::stable", "int"), functionId("::Other::stable", "int"));
    }

    TEST_F(FunctionDeclWrapperTest, UnnamedAndAdjustedParametersPreserveOrderAndQualifiers) {
        const auto functions = parse("void adjusted(int values[3], int callback(double), const int&, int = 1 + 2);");
        ASSERT_EQ(functions.size(), 1u);
        auto traits              = common();
        *traits.add_parameters() = parameter("values", "int *");
        *traits.add_parameters() = parameter("callback", "int (*)(double)");
        // A function type has no single declaration identity and is not a builtin type.
        traits.mutable_parameters(1)->mutable_type_ref()->set_is_builtin_or_template(false);
        *traits.add_parameters() = parameter("", "const int &");
        *traits.add_parameters() = parameter("", "int", "1 + 2");
        expectProto(serialize(functions[0])->common(), traits);
    }

    TEST_F(FunctionDeclWrapperTest, TypeReferencesKeepAliasSpellingAndResolveRegisteredDeclarations) {
        const auto functions = parse("namespace N { struct Node {}; using Alias = Node; const Alias* link(Node& value); }");
        ASSERT_EQ(functions.size(), 1u);
        auto* record = functions[0]->getParamDecl(0)->getType()->getPointeeType()->getAsTagDecl();
        ASSERT_NE(record, nullptr);
        const auto type_id = variableId("known-node-type");
        UEMeta::DeclDb::addDeclIdentity(record, type_id);
        auto traits              = common(ParserTypes::FUNCTION_KIND_FREE, "const ::N::Alias *");
        *traits.add_parameters() = parameter("value", "::N::Node &");
        auto expected_type = builtin("const ::N::Alias *");
        type_id.putProtoHash(expected_type.mutable_decl_id());
        *traits.mutable_return_type() = versionedRef(expected_type);
        type_id.putProtoHash(traits.mutable_parameters(0)->mutable_type_ref()->mutable_decl_id());
        expectProto(serialize(functions[0])->common(), traits);
    }

    TEST_F(FunctionDeclWrapperTest, UnresolvedTagsAreNotMarkedAsBuiltins) {
        const auto functions = parse("struct Unknown; Unknown* unresolved(Unknown& input);");
        ASSERT_EQ(functions.size(), 1u);
        auto traits = common(ParserTypes::FUNCTION_KIND_FREE, "::Unknown *");
        *traits.mutable_return_type()->mutable_is_builtin_or_template() = boolean(false);
        *traits.add_parameters() = parameter("input", "::Unknown &");
        traits.mutable_parameters(0)->mutable_type_ref()->set_is_builtin_or_template(false);
        expectProto(serialize(functions[0])->common(), traits);
    }

    TEST_F(FunctionDeclWrapperTest, FriendDefinitionIsAFreeFunctionInItsNamespace) {
        const auto functions = parse("namespace N { struct Owner { friend int friend_function(int x) { return x; } }; }");
        ASSERT_EQ(functions.size(), 1u);
        auto traits                         = common(ParserTypes::FUNCTION_KIND_FREE, "int");
        *traits.mutable_is_friend()         = boolean(true);
        *traits.add_parameters()            = parameter("x", "int");
        *traits.mutable_inline_definition() = versioned<ParserTypes::VersionedString>("{\n    return x;\n}\n");
        expectProto(*serialize(functions[0]), expectedFunction("::N::friend_function", "int", 0, traits));
    }

    TEST_F(FunctionDeclWrapperTest, DeletedFreeFunctionsAndDefaultedFriendComparisonsRetainDefinitionKind) {
        const auto functions = parse("void removed(int) = delete; struct Equal { friend bool operator==(const Equal&, const Equal&) = default; };");
        ASSERT_EQ(functions.size(), 2u);
        auto deleted = common();
        deleted.set_definition_kind(ParserTypes::FUNCTION_DEFINITION_DELETED);
        *deleted.add_parameters() = parameter("", "int");
        expectProto(serialize(functions[0])->common(), deleted);
        const auto* comparison = serialize(functions[1]);
        expectMetadata(comparison->metadata(), "::operator==");
        auto traits = common(ParserTypes::FUNCTION_KIND_FREE, "bool", ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                             ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        traits.set_definition_kind(ParserTypes::FUNCTION_DEFINITION_DEFAULTED);
        *traits.mutable_is_friend() = boolean(true);
        for (int index = 0; index < 2; ++index) {
            auto* argument = traits.add_parameters();
            *argument      = parameter("", "const ::Equal &");
            argument->mutable_type_ref()->set_is_builtin_or_template(false);
        }
        expectProto(comparison->common(), traits);
    }

    TEST_F(FunctionDeclWrapperTest, PendingDefaultsAndLateParsedBodiesRemainAbsent) {
        const auto functions = parse("void pending(int value = 7); template<class T> void delayed(T value);");
        ASSERT_EQ(functions.size(), 2u);
        functions[0]->getParamDecl(0)->setUnparsedDefaultArg();
        auto traits              = common();
        *traits.add_parameters() = parameter("value", "int");
        expectProto(serialize(functions[0])->common(), traits);
        functions[1]->setLateTemplateParsed(true);
        ASSERT_TRUE(functions[1]->doesThisDeclarationHaveABody());
        EXPECT_FALSE(serialize(functions[1])->common().has_inline_definition());
    }

    TEST_F(FunctionDeclWrapperTest, SynthesizedNonIdentifierParameterNamesUseTheirPrintedSpelling) {
        const auto functions = parse("void synthesized(int value);");
        ASSERT_EQ(functions.size(), 1u);
        // Parsed C++ parameters are identifiers. Use Clang's public mutation API to test
        // the fallback for a synthesized/recovery declaration with another name kind.
        auto& context = functions[0]->getASTContext();
        functions[0]->getParamDecl(0)->setDeclName(context.DeclarationNames.getCXXOperatorName(clang::OO_Plus));
        auto traits              = common();
        *traits.add_parameters() = parameter("operator+", "int");
        const auto* message      = serialize(functions[0]);
        expectProto(message->common(), traits);
        expectId(message->metadata().decl_id(), functionId("::synthesized", "int"));
    }

    TEST_F(FunctionDeclWrapperTest, PrimaryTemplatePreservesParametersDefaultsAndDependentFunctionTypes) {
        const auto functions = parse("template<class T, int N = 3> T select(T value);");
        ASSERT_EQ(functions.size(), 1u);
        auto traits                        = common(ParserTypes::FUNCTION_KIND_FREE, "T");
        *traits.add_parameters()           = parameter("value", "T");
        *traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_CLASS
                type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true }
            }
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_NON_TYPE
                name { versions { source_versions: "test-version" value: "N" } }
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
                value { versions { source_versions: "test-version" value: "3" } }
            }
        )pb");
        const auto* message                = serialize(functions[0]);
        expectProto(message->common(), traits);
        expectId(message->metadata().decl_id(), functionId("::select", "T<typenameint>"));
    }

    TEST_F(FunctionDeclWrapperTest, ExplicitSpecializationReferencesItsPrimaryAndIncludesConcreteArgumentsInIdentity) {
        const auto functions = parse("template<class T, int N> T select(T value); template<> int select<int, 2>(int value);");
        ASSERT_EQ(functions.size(), 2u);
        const auto primary_id = functionId("::select", "T<typenameint>");
        expectId(serialize(functions[0])->metadata().decl_id(), primary_id);
        auto traits                        = common(ParserTypes::FUNCTION_KIND_FREE, "int");
        *traits.add_parameters()           = parameter("value", "int");
        *traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            primary_template_decl_id { type_name { versions { source_versions: "test-version" value: "select" } } }
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE
                value { versions { source_versions: "test-version" value: "2" } }
            }
        )pb");
        primary_id.putProtoHash(traits.mutable_template_details()->mutable_primary_template_decl_id()->mutable_decl_id());
        const auto* message = serialize(functions[1]);
        expectProto(message->common(), traits);
        expectId(message->metadata().decl_id(), functionId("::select", "int<int2>"));
    }

    TEST_F(FunctionDeclWrapperTest, SpecializationWithoutRegisteredPrimaryDoesNotInventAReference) {
        const auto functions = parse("template<class T> void select(T); template<> void select<int>(int);");
        ASSERT_EQ(functions.size(), 2u);
        auto traits                        = common();
        *traits.add_parameters()           = parameter("", "int");
        *traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
        )pb");
        expectProto(serialize(functions[1])->common(), traits);
    }

    TEST_F(FunctionDeclWrapperTest, MethodsThroughBaseFunctionPointersKeepTheirKindsQualifiersAndRecordOwnership) {
        const auto functions = parse(R"cpp(struct Owner {
            Owner(); ~Owner(); explicit operator bool() const; static void create();
            void cv() const volatile &; void move() &&; template<class T> void choose(T);
        }; template<> void Owner::choose<int>(int);)cpp");
        ASSERT_EQ(functions.size(), 8u);
        const std::vector<std::string> names{"Owner", "~Owner", "operator bool", "create", "cv", "move", "choose", "choose"};
        const std::vector<std::string> signatures{"", "", " const", "", " const volatile &", " &&", "T<typename>", "int<int>"};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(index);
            auto traits = common(ParserTypes::FUNCTION_KIND_MEMBER);
            traits.set_definition_kind(ParserTypes::FUNCTION_DEFINITION_NORMAL);
            if (index < 2) {
                traits.set_kind(index == 0 ? ParserTypes::FUNCTION_KIND_CONSTRUCTOR : ParserTypes::FUNCTION_KIND_DESTRUCTOR);
                traits.clear_return_type();
                if (index == 0)
                    *traits.mutable_is_explicit() = boolean(false);
            }
            else if (index == 2) {
                traits.set_kind(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION);
                *traits.mutable_return_type() = versionedRef(builtin("bool"));
                *traits.mutable_is_explicit()                                       = boolean(true);
            }
            else if (index == 3) {
                traits.set_kind(ParserTypes::FUNCTION_KIND_STATIC_MEMBER);
                *traits.mutable_storage_class() = versioned<ParserTypes::VersionedFunctionStorageClass>(ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC);
            }
            else if (index == 6) {
                *traits.add_parameters()           = parameter("", "T");
                *traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
                    parameters {
                        kind: TEMPLATE_PARAMETER_KIND_CLASS
                        type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true }
                    }
                )pb");
            }
            else if (index == 7) {
                *traits.add_parameters()           = parameter("", "int");
                *traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
                    specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
                    specialized_parameters {
                        kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                        type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
                    }
                )pb");
            }
            // serialize accepts FunctionDecl*, so this exercises the base-pointer instantiation, not MethodDeclWrapper.
            expectProto(*serialize(functions[index]), expectedFunction("::Owner::" + names[index], signatures[index], index, traits));
            expectUnregistered(functions[index]);
        }
    }

    TEST_F(FunctionDeclWrapperTest, ImplicitAndExplicitInstantiationsPreserveKindArgumentsPrimaryAndAvailableBodies) {
        const auto functions = parse(R"cpp(
            template<class T> T twice(T value) { return value + value; }
            template int twice<int>(int);
            extern template long twice<long>(long);
            double used = twice(1.0);
        )cpp");
        ASSERT_FALSE(functions.empty());
        auto* primary = functions[0]->getDescribedFunctionTemplate();
        ASSERT_NE(primary, nullptr);
        const auto primary_id = functionId("::twice", "T<typename>");
        expectId(serialize(functions[0])->metadata().decl_id(), primary_id);
        int count = 0;
        for (auto* specialization : primary->specializations()) {
            const auto kind             = specialization->getTemplateSpecializationKind();
            const auto implicit         = kind == clang::TSK_ImplicitInstantiation;
            const auto declaration_only = kind == clang::TSK_ExplicitInstantiationDeclaration;
            ASSERT_TRUE(implicit || declaration_only || kind == clang::TSK_ExplicitInstantiationDefinition);
            const std::string type = implicit ? "double" : declaration_only ? "long" : "int";
            SCOPED_TRACE(type);
            auto traits              = common(ParserTypes::FUNCTION_KIND_FREE, type);
            *traits.add_parameters() = parameter("value", type);
            if (!declaration_only)
                *traits.mutable_inline_definition() = versioned<ParserTypes::VersionedString>("{\n    return value + value;\n}\n");
            auto* details = traits.mutable_template_details();
            details->set_specialization_kind(implicit           ? ParserTypes::TEMPLATE_SPECIALIZATION_IMPLICIT
                                             : declaration_only ? ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION
                                                                : ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
            *details->mutable_primary_template_decl_id()->mutable_type_name() = versioned<ParserTypes::VersionedString>("twice");
            primary_id.putProtoHash(details->mutable_primary_template_decl_id()->mutable_decl_id());
            auto* argument = details->add_specialized_parameters();
            argument->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
            *argument->mutable_type() = builtin(type);
            const auto* message       = serialize(specialization);
            expectProto(message->common(), traits);
            expectId(message->metadata().decl_id(), functionId("::twice", type + "<" + type + ">"));
            ++count;
        }
        EXPECT_EQ(count, 3);
    }

    TEST_F(FunctionDeclWrapperTest, ForwardTypeReferencesRetainTheirOccurrenceInReturnAndParameterTypes) {
        const auto functions = parse("struct Node {}; Node* forward(Node& input);");
        ASSERT_EQ(functions.size(), 1u);
        auto* record = functions[0]->getReturnType()->getPointeeType()->getAsTagDecl()->getDefinition();
        ASSERT_NE(record, nullptr);
        UEMeta::DeclDb::addForwardDeclaration(record);
        const auto reference = UEMeta::DeclDb::queryDeclIdentity(record);
        ASSERT_TRUE(std::holds_alternative<uint64_t>(reference));
        EXPECT_EQ(std::get<uint64_t>(reference), 0u);
        auto traits              = common(ParserTypes::FUNCTION_KIND_FREE, "::Node *");
        *traits.add_parameters() = parameter("input", "::Node &");
        traits.mutable_return_type()->clear_is_builtin_or_template();
        *traits.mutable_return_type()->mutable_forward_decl_index() = versioned<ParserTypes::VersionedUint64>(0);
        traits.mutable_parameters(0)->mutable_type_ref()->set_forward_decl_index(0);
        expectProto(*serialize(functions[0]), expectedFunction("::forward", "::Node &", 1, traits));
    }

    TEST_F(FunctionDeclWrapperTest, StaticAndMemberToFileWriteOnlyFunctionFilesWithoutReadingThemBack) {
        const auto functions = parse("void written(); void member_written();");
        ASSERT_EQ(functions.size(), 2u);
        const auto* message = serialize(functions[0]);
        const auto  path    = outputPath(message->metadata(), "functionbin");
        ASSERT_FALSE(std::filesystem::exists(path));
        FunctionDeclWrapper<>::toFile(message, arena);
        expectOutput(path);
        const auto before = outputFiles();
        FunctionDeclWrapper{functions[1], arena}.toFile();
        const auto                         after = outputFiles();
        std::vector<std::filesystem::path> added;
        std::set_difference(after.begin(), after.end(), before.begin(), before.end(), std::back_inserter(added));
        ASSERT_EQ(added.size(), 1u);
        EXPECT_EQ(added[0].extension(), ".functionbin");
        expectOutput(added[0]);
    }

    TEST_F(FunctionDeclWrapperTest, ToFileRejectsMissingInputsAndPropagatesOpenFailures) {
        EXPECT_THROW(FunctionDeclWrapper<>::toFile(nullptr, arena), std::invalid_argument);
        const auto functions = parse("void cannot_write();");
        ASSERT_EQ(functions.size(), 1u);
        const auto* message = serialize(functions[0]);
        EXPECT_THROW(FunctionDeclWrapper<>::toFile(message, nullptr), std::invalid_argument);
        const auto path = outputPath(message->metadata(), "functionbin");
        ASSERT_TRUE(std::filesystem::create_directory(path));
        EXPECT_THROW(FunctionDeclWrapper<>::toFile(message, arena), std::runtime_error);
        EXPECT_TRUE(std::filesystem::remove(path));
    }
} // namespace
