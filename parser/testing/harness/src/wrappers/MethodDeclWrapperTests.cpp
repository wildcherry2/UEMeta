#include "CallableTest.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "UEMeta/clang/wrappers/FunctionDeclWrapper.hpp"
#include "clang/Basic/TargetInfo.h"

namespace {
    using namespace UEMeta::Testing;
    using UEMeta::MethodDeclWrapper;

    class MethodDeclWrapperTest : public CallableTest {
    protected:
        ParserTypes::MemberFunction* serialize(clang::FunctionDecl* declaration, bool known_layout = false) const {
            auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration);
            if (!method) {
                ADD_FAILURE() << "Expected a method";
                return nullptr;
            }
            return MethodDeclWrapper{method, arena}.serialize(known_layout);
        }

        static ParserTypes::MemberFunction expectedMethod(std::string_view name, const UEMeta::Hash& identity, ParserTypes::FunctionCommon traits,
                                                          ParserTypes::AccessSpecifier access = ParserTypes::ACCESS_SPECIFIER_PUBLIC,
                                                          bool is_const = false, bool is_volatile = false, bool is_deleted = false,
                                                          ParserTypes::FunctionVirtuality virtuality = ParserTypes::FUNCTION_VIRTUALITY_NONE) {
            ParserTypes::MemberFunction message;
            message.set_name(name);
            message.mutable_func_id()->set_a(identity.a);
            message.mutable_func_id()->set_b(identity.b);
            *message.mutable_common()      = std::move(traits);
            *message.mutable_access()      = versioned<ParserTypes::VersionedAccessSpecifier>(access);
            *message.mutable_is_const()    = boolean(is_const);
            *message.mutable_is_volatile() = boolean(is_volatile);
            *message.mutable_is_deleted()  = boolean(is_deleted);
            *message.mutable_virtuality()  = versioned<ParserTypes::VersionedFunctionVirtualityKind>(virtuality);
            return message;
        }

        static ParserTypes::FunctionCommon methodCommon(ParserTypes::FunctionKind         kind = ParserTypes::FUNCTION_KIND_MEMBER,
                                                        std::optional<std::string_view>   return_type = "void",
                                                        ParserTypes::FunctionStorageClass storage = ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED) {
            auto traits = common(kind, return_type, storage);
            traits.set_definition_kind(ParserTypes::FUNCTION_DEFINITION_NORMAL);
            return traits;
        }

        static void location(ParserTypes::MemberFunction& expected, uint64_t index, int64_t offset) {
            *expected.mutable_vtable_index()  = versioned<ParserTypes::VersionedUint64>(index);
            *expected.mutable_vtable_offset() = versioned<ParserTypes::VersionedInt64>(offset);
        }
    };

    TEST_F(MethodDeclWrapperTest, MethodPreservesCompleteCommonDataQualifiersAccessArenaAndOwnership) {
        const auto functions = parse("namespace N { class Owner { public: int read(int index = 2) const volatile &; }; }");
        ASSERT_EQ(functions.size(), 1u);
        auto traits              = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "int");
        *traits.add_parameters() = parameter("index", "int", "2");
        const auto expected      =
            expectedMethod("read", functionId("::N::Owner::read", "int const volatile &"), traits, ParserTypes::ACCESS_SPECIFIER_PUBLIC, true, true);
        const auto  before  = outputFiles();
        const auto* message = serialize(functions[0], true);
        ASSERT_NE(message, nullptr);
        EXPECT_EQ(message->GetArena(), arena.get());
        EXPECT_EQ(message->common().GetArena(), arena.get());
        expectProto(*message, expected);
        expectUnregistered(functions[0]);
        EXPECT_EQ(UEMeta::DeclDb::queryDecl(functionId("::N::Owner::read", "int const volatile &")), nullptr);
        EXPECT_EQ(outputFiles(), before);
    }

    TEST_F(MethodDeclWrapperTest, ExplicitAndDefaultAccessArePreservedIncludingFallbackForUnsetAccess) {
        const auto functions = parse(R"cpp(
            struct Structure { void default_public(); protected: void protected_method(); private: void private_method(); };
            class Class { void default_private(); public: void public_method(); };
        )cpp");
        ASSERT_EQ(functions.size(), 5u);
        const std::vector<std::string> names{"default_public", "protected_method", "private_method", "default_private", "public_method"};
        const std::vector<std::string> owners{"Structure", "Structure", "Structure", "Class", "Class"};
        const std::vector<ParserTypes::AccessSpecifier> access{
            ParserTypes::ACCESS_SPECIFIER_PUBLIC, ParserTypes::ACCESS_SPECIFIER_PROTECTED,
            ParserTypes::ACCESS_SPECIFIER_PRIVATE, ParserTypes::ACCESS_SPECIFIER_PRIVATE,
            ParserTypes::ACCESS_SPECIFIER_PUBLIC
        };
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(names[index]);
            const auto expected = expectedMethod(names[index], functionId("::" + owners[index] + "::" + names[index]), methodCommon(), access[index]);
            expectProto(*serialize(functions[index]), expected);
            if (index == 0 || index == 3) {
                // Exercise the documented parent-kind fallback on an AST with unset access.
                functions[index]->setAccess(clang::AS_none);
                expectProto(*serialize(functions[index]), expected);
            }
        }
    }

    TEST_F(MethodDeclWrapperTest, ConstructorsDestructorsConversionsAndStaticMembersHaveDistinctCompleteRepresentations) {
        const auto functions = parse(R"cpp(struct Owner {
            Owner(); explicit Owner(int value); explicit(false) Owner(double value); ~Owner();
            explicit operator bool() const; operator int() volatile; static int create(int value);
        };)cpp");
        ASSERT_EQ(functions.size(), 7u);
        const std::vector<std::string> names{"Owner", "Owner", "Owner", "~Owner", "operator bool", "operator int", "create"};
        const std::vector<std::string> signatures{"", "int", "double", "", " const", " volatile", "int"};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(index);
            ParserTypes::FunctionCommon traits;
            if (index < 3) {
                traits                        = methodCommon(ParserTypes::FUNCTION_KIND_CONSTRUCTOR, std::nullopt);
                *traits.mutable_is_explicit() = boolean(index == 1);
                if (index != 0)
                    *traits.add_parameters() = parameter("value", index == 1 ? "int" : "double");
            }
            else if (index == 3)
                traits = methodCommon(ParserTypes::FUNCTION_KIND_DESTRUCTOR, std::nullopt);
            else if (index < 6) {
                traits                        = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION, index == 4 ? "bool" : "int");
                *traits.mutable_is_explicit() = boolean(index == 4);
            }
            else {
                traits                   = methodCommon(ParserTypes::FUNCTION_KIND_STATIC_MEMBER, "int", ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC);
                *traits.add_parameters() = parameter("value", "int");
            }
            const auto expected = expectedMethod(names[index], functionId("::Owner::" + names[index], signatures[index]), traits,
                                                 ParserTypes::ACCESS_SPECIFIER_PUBLIC, index == 4, index == 5);
            expectProto(*serialize(functions[index]), expected);
        }
    }

    TEST_F(MethodDeclWrapperTest, ConversionNameUsesTheQualifiedCanonicalTargetNotAnAlias) {
        const auto functions =
            parse("namespace A { struct Result {}; using Alias = Result; } namespace B { struct Source { operator A::Alias() const; }; }");
        ASSERT_EQ(functions.size(), 1u);
        auto traits = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION, "::A::Alias");
        traits.mutable_return_type()->mutable_versions(0)->mutable_value()->set_is_builtin_or_template(false);
        *traits.mutable_is_explicit() = boolean(false);
        const auto expected           = expectedMethod("operator ::A::Result", functionId("::B::Source::operator ::A::Result", " const"), traits,
                                                       ParserTypes::ACCESS_SPECIFIER_PUBLIC, true);
        expectProto(*serialize(functions[0]), expected);
    }

    TEST_F(MethodDeclWrapperTest, CvAndReferenceQualifiersDistinguishOtherwiseIdenticalOverloads) {
        const auto functions = parse("struct Owner { int value() &; int value() const &; int value() volatile &; int value() &&; };");
        ASSERT_EQ(functions.size(), 4u);
        const std::vector<std::string> signatures{" &", " const &", " volatile &", " &&"};
        std::vector<UEMeta::Hash>      ids;
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(index);
            const auto id       = functionId("::Owner::value", signatures[index]);
            const auto expected = expectedMethod("value", id, methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "int"),
                                                 ParserTypes::ACCESS_SPECIFIER_PUBLIC, index == 1, index == 2);
            expectProto(*serialize(functions[index]), expected);
            EXPECT_EQ(std::find(ids.begin(), ids.end(), id), ids.end());
            ids.push_back(id);
        }
    }

    TEST_F(MethodDeclWrapperTest, DeletedAndDefaultedMembersRetainDefinitionAndEvaluationFlags) {
        const auto functions = parse("struct Owner { constexpr Owner() = default; constexpr ~Owner() = default; void removed() = delete; };");
        ASSERT_EQ(functions.size(), 3u);
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(index);
            const auto kind = index == 0
                                  ? ParserTypes::FUNCTION_KIND_CONSTRUCTOR
                                  : index == 1
                                  ? ParserTypes::FUNCTION_KIND_DESTRUCTOR
                                  : ParserTypes::FUNCTION_KIND_MEMBER;
            auto traits = methodCommon(kind, index < 2 ? std::nullopt : std::optional<std::string_view>{"void"});
            traits.set_definition_kind(index < 2 ? ParserTypes::FUNCTION_DEFINITION_DEFAULTED : ParserTypes::FUNCTION_DEFINITION_DELETED);
            *traits.mutable_consteval_kind() = versioned<ParserTypes::VersionedConstantEvaluationKind>(
                index < 2 ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE);
            if (index == 0)
                *traits.mutable_is_explicit() = boolean(false);
            const std::string name = index == 0 ? "Owner" : index == 1 ? "~Owner" : "removed";
            expectProto(*serialize(functions[index]),
                        expectedMethod(name, functionId("::Owner::" + name), traits, ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false, index == 2));
        }
    }

    TEST_F(MethodDeclWrapperTest, VirtualMethodsOmitUnknownLocationsAndIncludeExactKnownSlots) {
        const auto functions = parse("struct Base { virtual void first(); virtual void pure() = 0; virtual ~Base(); };");
        ASSERT_EQ(functions.size(), 3u);
        const std::vector<std::string> names{"first", "pure", "~Base"};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            SCOPED_TRACE(index);
            auto traits   = index == 2 ? methodCommon(ParserTypes::FUNCTION_KIND_DESTRUCTOR, std::nullopt) : methodCommon();
            auto expected =
                expectedMethod(names[index], functionId("::Base::" + names[index]), traits, ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false, false,
                               index == 1 ? ParserTypes::FUNCTION_VIRTUALITY_PURE : ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL);
            expectProto(*serialize(functions[index], false), expected);
            location(expected, index, 0);
            expectProto(*serialize(functions[index], true), expected);
        }
    }

    TEST_F(MethodDeclWrapperTest, OverridesUseTheSecondaryBaseVfptrOffset) {
        const auto functions = parse(
            "struct Left { virtual void left(); }; struct Right { virtual void right(); }; struct Derived : Left, Right { void right() override; };");
        ASSERT_EQ(functions.size(), 3u);
        auto expected = expectedMethod("right", functionId("::Derived::right"), methodCommon(), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false,
                                       false, ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL);
        // On Windows x64 each base contributes one eight-byte vfptr.
        location(expected, 0, 8);
        expectProto(*serialize(functions[2], true), expected);
    }

    TEST_F(MethodDeclWrapperTest, VirtualBaseLocationsAreRelativeToTheCompleteRecord) {
        const auto functions = parse("struct Base { virtual void run(); }; struct Derived : virtual Base { void run() override; };");
        ASSERT_EQ(functions.size(), 2u);
        auto expected = expectedMethod("run", functionId("::Derived::run"), methodCommon(), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false, false,
                                       ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL);
        // The vbptr occupies the first eight bytes; Base's vfptr follows it.
        location(expected, 0, 8);
        expectProto(*serialize(functions[1], true), expected);
    }

    TEST_F(MethodDeclWrapperTest, DeletingDestructorSlotWorksWithTheOlderClangWindowsAbi) {
        const auto functions = parse("struct Lifetime { virtual ~Lifetime(); };", {"-fclang-abi-compat=21"});
        ASSERT_EQ(functions.size(), 1u);
        const auto& context = functions[0]->getASTContext();
        ASSERT_FALSE(context.getTargetInfo().emitVectorDeletingDtors(context.getLangOpts()));
        auto expected =
            expectedMethod("~Lifetime", functionId("::Lifetime::~Lifetime"), methodCommon(ParserTypes::FUNCTION_KIND_DESTRUCTOR, std::nullopt),
                           ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false, false, ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL);
        location(expected, 0, 0);
        expectProto(*serialize(functions[0], true), expected);
    }

    TEST_F(MethodDeclWrapperTest, WindowsGnuAbiIsRejectedOnlyWhenVtableDetailsAreRequested) {
        // This remains a Windows target, but uses the unsupported Itanium-family ABI.
        const auto functions = parse("struct Owner { virtual void run(); };", {"--target=x86_64-w64-windows-gnu"});
        ASSERT_EQ(functions.size(), 1u);
        auto expected = expectedMethod("run", functionId("::Owner::run"), methodCommon(), ParserTypes::ACCESS_SPECIFIER_PUBLIC, false, false, false,
                                       ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL);
        expectProto(*serialize(functions[0], false), expected);
        using UnsupportedAbi = UEMeta::DeclException<clang::CXXMethodDecl>;
        EXPECT_THROW((void)serialize(functions[0], true), UnsupportedAbi);
    }

    TEST_F(MethodDeclWrapperTest, DependentVirtualMethodsPreserveTypesWithoutInventingLayout) {
        const auto functions = parse("template<class T> struct Owner { virtual T get() const = 0; };");
        ASSERT_EQ(functions.size(), 1u);
        auto expected = expectedMethod("get", functionId("::Owner<T>::get", " const"), methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "T"),
                                       ParserTypes::ACCESS_SPECIFIER_PUBLIC, true, false, false, ParserTypes::FUNCTION_VIRTUALITY_PURE);
        expectProto(*serialize(functions[0]), expected);
    }

    TEST_F(MethodDeclWrapperTest, MemberTemplatesAndSpecializationsDoNotReferenceTopLevelFunctionIds) {
        const auto functions = parse("struct Owner { template<class T> T select(T value); }; template<> int Owner::select<int>(int value);");
        ASSERT_EQ(functions.size(), 2u);
        auto primary_traits                        = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "T");
        *primary_traits.add_parameters()           = parameter("value", "T");
        *primary_traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            parameters {
                kind: TEMPLATE_PARAMETER_KIND_CLASS
                type { type_name { versions { source_versions: "test-version" value: "T" } } is_builtin_or_template: true }
            }
        )pb");
        const auto primary_id                      = functionId("::Owner::select", "T<typename>");
        expectProto(*serialize(functions[0]), expectedMethod("select", primary_id, primary_traits));
        expectUnregistered(functions[0]);
        // Even if a caller erroneously registers a member, no top-level reference should leak into its specialization.
        UEMeta::DeclDb::addDeclIdentity(functions[0], primary_id);
        auto specialized_traits                        = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "int");
        *specialized_traits.add_parameters()           = parameter("value", "int");
        *specialized_traits.mutable_template_details() = proto<ParserTypes::TemplateDetails>(R"pb(
            specialization_kind: TEMPLATE_SPECIALIZATION_EXPLICIT
            specialized_parameters {
                kind: TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE
                type { type_name { versions { source_versions: "test-version" value: "int" } } is_builtin_or_template: true }
            }
        )pb");
        expectProto(*serialize(functions[1]), expectedMethod("select", functionId("::Owner::select", "int<int>"), specialized_traits));
        expectUnregistered(functions[1]);
    }

    TEST_F(MethodDeclWrapperTest, InheritedTopLevelRepresentationStillDoesNotRegisterAMemberIdentity) {
        const auto functions = parse("struct Owner { int read() const; };");
        ASSERT_EQ(functions.size(), 1u);
        auto*                                  method     = llvm::cast<clang::CXXMethodDecl>(functions[0]);
        const auto                             occurrence = UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() + 1;
        const auto*                            message    = MethodDeclWrapper{method, arena}.toIntermediateRepresentation();
        ParserTypes::TLFreeFunctionDeclaration expected;
        *expected.mutable_metadata() = metadata("::Owner::read", functionId("::Owner::read", " const"), occurrence);
        *expected.mutable_common()   = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "int");
        expectProto(*message, expected);
        expectUnregistered(method);
    }

    TEST_F(MethodDeclWrapperTest, MethodDefinitionsPreserveBodiesDefaultsAndConstantEvaluation) {
        const auto functions = parse(R"cpp(struct Owner {
            constexpr int constant(int value = 2) const { return value + 1; }
            consteval int immediate(int value) const { return value * 2; }
        };)cpp");
        ASSERT_EQ(functions.size(), 2u);
        const std::vector<std::string> names{"constant", "immediate"};
        const std::vector<std::string> bodies{"{\n    return value + 1;\n}\n", "{\n    return value * 2;\n}\n"};
        for (std::size_t index = 0; index < functions.size(); ++index) {
            auto traits                         = methodCommon(ParserTypes::FUNCTION_KIND_MEMBER, "int");
            *traits.add_parameters()            = parameter("value", "int", index == 0 ? std::optional<std::string_view>{"2"} : std::nullopt);
            *traits.mutable_inline_definition() = versioned<ParserTypes::VersionedString>(bodies[index]);
            *traits.mutable_consteval_kind()    = versioned<ParserTypes::VersionedConstantEvaluationKind>(
                index == 0 ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_CONSTEVAL);
            expectProto(*serialize(functions[index]), expectedMethod(names[index], functionId("::Owner::" + names[index], "int const"), traits,
                                                                     ParserTypes::ACCESS_SPECIFIER_PUBLIC, true));
        }
    }
} // namespace
