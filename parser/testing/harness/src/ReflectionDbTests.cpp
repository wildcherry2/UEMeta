#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "UEMeta/Cli.hpp"
#include "UEMeta/clang/MetaPreprocessor.hpp"
#include "UEMeta/clang/ReflectionDb.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Stmt.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/VirtualFileSystem.h"

namespace {
    using UEMeta::Config;
    using UEMeta::ReflectionDb;
    using Kind  = ParserTypes::ReflectionKind;
    using Check = std::function<void(clang::ASTContext&)>;

    // Run the real preprocessor and ReflectionDb against disposable, standalone
    // C++ files. No Unreal headers, wrappers, DeclDb, or serialization participate.
    class ReflectionAction final : public clang::ASTFrontendAction {
    public:
        ReflectionAction(Check check, bool record_macros) : check(std::move(check)), record_macros(record_macros) {}

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef) override {
            if (record_macros)
                compiler.getPreprocessor().addPPCallbacks(
                    std::make_unique<UEMeta::MetaPreprocessor>(compiler.getSourceManager(), compiler.getLangOpts()));
            class Consumer final : public clang::ASTConsumer {
            public:
                explicit Consumer(Check check) : check(std::move(check)) {}
                void HandleTranslationUnit(clang::ASTContext& context) override {
                    ASSERT_FALSE(context.getDiagnostics().hasErrorOccurred()) << "Invalid reflection fixture";
                    check(context);
                }

            private:
                Check check;
            };
            return std::make_unique<Consumer>(check);
        }

    private:
        Check check;
        bool  record_macros;
    };

    class ReflectionDbTest : public ::testing::Test {
    protected:
        std::filesystem::path temporary_root;
        std::filesystem::path root;
        bool                  previous_unreal_enabled{};

        void SetUp() override {
            previous_unreal_enabled = Config::getConfig().unrealExtensionsEnabled();
            Config::getConfig().setUnrealExtensionsForTesting(true);
            ReflectionDb::reset();
            llvm::SmallString<128> directory;
            const auto             error = llvm::sys::fs::createUniqueDirectory("uemeta-reflection-tests", directory);
            ASSERT_FALSE(error) << error.message();
            temporary_root = directory.str().str();
            // Package searches stop here even in the intentionally missing-package cases.
            root = temporary_root / Config::getConfig().getFileDelimiter();
            write("Module/DummyModule.Build.cs", "// A dummy package marker, never compiled.\n");
            write("Macros.hpp", R"cpp(
                #define UCLASS(...)
                #define USTRUCT(...)
                #define UINTERFACE(...)
                #define UENUM(...)
                #define UFUNCTION(...)
                #define UPROPERTY(...)
                #define GENERATED_BODY(...)
                #define UNRELATED(...)
                #define DUMMY_API
            )cpp");
        }

        void TearDown() override {
            ReflectionDb::reset();
            Config::getConfig().setUnrealExtensionsForTesting(previous_unreal_enabled);
            if (!temporary_root.empty()) {
                std::error_code error;
                std::filesystem::remove_all(temporary_root, error);
                EXPECT_FALSE(error) << error.message();
            }
        }

        std::filesystem::path write(std::filesystem::path relative, std::string_view code) const {
            const auto path = root / relative;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream output{path, std::ios::binary};
            output << code;
            output.close();
            EXPECT_TRUE(output.good()) << path;
            return path;
        }

        void parse(std::string_view code, const Check& check, std::string_view relative = "Module/Public/Fixture.cpp", bool record_macros = true,
                   const std::vector<std::string>& extra_arguments = {}) {
            ReflectionDb::reset();
            const auto               path = write(relative, code);
            std::vector<std::string> arguments{
                "-std=c++20", "-fsyntax-only", "-nostdinc", "--target=x86_64-pc-windows-msvc", "-include", (root / "Macros.hpp").string()};
            arguments.insert(arguments.end(), extra_arguments.begin(), extra_arguments.end());
            const clang::tooling::FixedCompilationDatabase commands{root.string(), arguments};
            clang::tooling::ClangTool                      tool{commands, {path.string()}};
            class Factory final : public clang::tooling::FrontendActionFactory {
            public:
                Factory(Check check, bool record_macros) : check(std::move(check)), record_macros(record_macros) {}
                std::unique_ptr<clang::FrontendAction> create() override { return std::make_unique<ReflectionAction>(check, record_macros); }

            private:
                Check check;
                bool  record_macros;
            };
            bool    checked = false;
            Factory factory{[&](clang::ASTContext& context) {
                                checked = true;
                                check(context);
                            },
                            record_macros};
            EXPECT_EQ(tool.run(&factory), 0) << code;
            EXPECT_TRUE(checked);
            // Clear borrowed AST pointers before the next compilation can reuse them.
            ReflectionDb::reset();
        }

        static clang::NamedDecl* find(clang::DeclContext* scope, std::string_view name, unsigned& occurrence) {
            for (auto* declaration : scope->decls()) {
                if (declaration->isImplicit())
                    continue;
                if (auto* named = llvm::dyn_cast<clang::NamedDecl>(declaration); named && named->getQualifiedNameAsString() == name) {
                    if (occurrence == 0)
                        return named;
                    --occurrence;
                }
                if (auto* nested = llvm::dyn_cast<clang::DeclContext>(declaration)) {
                    if (auto* result = find(nested, name, occurrence))
                        return result;
                }
            }
            return nullptr;
        }

        template <typename T = clang::NamedDecl>
        static T* find(clang::ASTContext& context, std::string_view name, unsigned occurrence = 0) {
            auto* result = llvm::dyn_cast_or_null<T>(find(context.getTranslationUnitDecl(), name, occurrence));
            EXPECT_NE(result, nullptr) << name << " (occurrence " << occurrence << ')';
            return result;
        }

        static std::string_view package(const clang::Decl* declaration) {
            if (!declaration) {
                ADD_FAILURE() << "Missing reflection fixture declaration";
                return {};
            }
            if (auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration))
                return ReflectionDb::registerReflectable(record);
            if (auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration))
                return ReflectionDb::registerReflectable(method);
            if (auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration))
                return ReflectionDb::registerReflectable(enumeration);
            if (auto* field = llvm::dyn_cast<clang::FieldDecl>(declaration))
                return ReflectionDb::registerReflectable(field);
            if (auto* space = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                return ReflectionDb::registerReflectable(space);
            ADD_FAILURE() << "Unsupported reflection fixture declaration";
            return {};
        }

        static void expectPackages(clang::ASTContext& context, const std::vector<std::pair<std::string, std::string>>& expectations) {
            for (const auto& [name, expected] : expectations) {
                SCOPED_TRACE(name);
                auto* declaration = find(context, name);
                ASSERT_NE(declaration, nullptr);
                EXPECT_EQ(package(declaration), expected);
                EXPECT_EQ(package(declaration), expected); // Positive and negative results are idempotent.
            }
        }

        static void add(clang::ASTContext& context, Kind kind, unsigned begin, unsigned end, clang::FileID file = {}) {
            auto& source = context.getSourceManager();
            ReflectionDb::addReflectionMacro(file.isValid() ? file : source.getMainFileID(), kind, begin, end, source);
        }
    };

    using DeclarationTypes = ::testing::Types<clang::RecordDecl, clang::EnumDecl, clang::NamespaceDecl, clang::CXXMethodDecl, clang::FieldDecl>;
    template <typename T>
    class ReflectionNullTest : public ReflectionDbTest {};
    TYPED_TEST_SUITE(ReflectionNullTest, DeclarationTypes);

    TYPED_TEST(ReflectionNullTest, NullIsNeverReflected) {
        EXPECT_TRUE(ReflectionDb::registerReflectable(static_cast<const TypeParam*>(nullptr)).empty());
        Config::getConfig().setUnrealExtensionsForTesting(false);
        EXPECT_TRUE(ReflectionDb::registerReflectable(static_cast<const TypeParam*>(nullptr)).empty());
    }

    class ReflectionKindTest : public ReflectionDbTest, public ::testing::WithParamInterface<std::tuple<std::string, int>> {};

    TEST_P(ReflectionKindTest, OnlyCompatibleDeclarationKindsAreAccepted) {
        const auto& [macro, declaration_kind] = GetParam();
        const std::vector<std::string> declarations{
            macro + "() struct Target {};", macro + "() enum class Target { Value };", macro + "() namespace Target { enum Type { Value }; }",
            "struct Owner { " + macro + "() void Target(); };", "struct Owner { " + macro + "() int Target; };"};
        const bool compatible = declaration_kind == 0   ? macro == "UCLASS" || macro == "USTRUCT" || macro == "UINTERFACE"
                                : declaration_kind <= 2 ? macro == "UENUM"
                                : declaration_kind == 3 ? macro == "UFUNCTION"
                                                        : macro == "UPROPERTY";
        parse(declarations[declaration_kind], [&](clang::ASTContext& context) {
            auto* target = find(context, declaration_kind >= 3 ? "Owner::Target" : "Target");
            ASSERT_NE(target, nullptr);
            if (compatible) {
                EXPECT_EQ(package(target), "DummyModule");
                EXPECT_EQ(package(target), "DummyModule");
                if (declaration_kind == 2)
                    EXPECT_EQ(package(find(context, "Target::Type")), "DummyModule");
            }
            else {
                try {
                    (void)package(target);
                    FAIL() << "An incompatible annotation must throw";
                }
                catch (const UEMeta::DeclException<>& error) {
                    EXPECT_NE(std::string{error.what()}.find("Reflection macro found, but it was recorded as type"), std::string::npos);
                }
            }
        });
    }

    INSTANTIATE_TEST_SUITE_P(AllMacros, ReflectionKindTest,
                             ::testing::Combine(::testing::Values("UCLASS", "USTRUCT", "UINTERFACE", "UENUM", "UFUNCTION", "UPROPERTY"),
                                                ::testing::Range(0, 5)),
                             ([](const auto& info) {
                                 const char* names[]{"Record", "Enum", "Namespace", "Method", "Field"};
                                 return std::get<0>(info.param) + "_" + names[std::get<1>(info.param)];
                             }));

    TEST_F(ReflectionDbTest, UnannotatedDeclarationsAndInactiveMacrosAreNotReflected) {
        parse(R"cpp(
            // UCLASS() struct Comment {};
            const char* text = "UENUM() enum Text {}";
            #if 0
            USTRUCT() struct Inactive {};
            #endif
            UNRELATED() struct Plain {
                GENERATED_BODY()
                int field;
                void method();
            };
            enum class PlainEnum { Value };
            namespace PlainNamespace { enum Type { Value }; }
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Plain", ""},
                                           {"Plain::field", ""},
                                           {"Plain::method", ""},
                                           {"PlainEnum", ""},
                                           {"PlainNamespace", ""},
                                           {"PlainNamespace::Type", ""}});
              });
    }

    TEST_F(ReflectionDbTest, DisabledExtensionsIgnoreMacrosDeclarationsAndPreviouslyCachedResults) {
        parse(R"cpp(
            UCLASS() struct Record { UPROPERTY() int field; UFUNCTION() void method(); };
            UENUM() enum class Enum { Value };
            UENUM() namespace Legacy { enum Type { Value }; }
        )cpp",
              [](clang::ASTContext& context) {
                  const std::vector<std::pair<std::string, std::string>> reflected{{"Record", "DummyModule"},
                                                                                   {"Record::field", "DummyModule"},
                                                                                   {"Record::method", "DummyModule"},
                                                                                   {"Enum", "DummyModule"},
                                                                                   {"Legacy", "DummyModule"}};
                  expectPackages(context, reflected);
                  Config::getConfig().setUnrealExtensionsForTesting(false);
                  for (const auto& [name, ignored] : reflected)
                      EXPECT_TRUE(package(find(context, name)).empty());
                  ReflectionDb::reset();
                  // An invalid FileID would throw if disabled registration touched the source manager.
                  EXPECT_NO_THROW(ReflectionDb::addReflectionMacro({}, ParserTypes::REFLECTION_KIND_UCLASS, 0, 1, context.getSourceManager()));
                  Config::getConfig().setUnrealExtensionsForTesting(true);
                  for (const auto& [name, ignored] : reflected)
                      EXPECT_TRUE(package(find(context, name)).empty());
              });
        Config::getConfig().setUnrealExtensionsForTesting(false);
        parse(
            "UCLASS() struct NoPackage {};", [](clang::ASTContext& context) { expectPackages(context, {{"NoPackage", ""}}); },
            "NoPackage/Disabled.cpp");
    }

    TEST_F(ReflectionDbTest, MatchesAdjacentMultilineAndAttributedDeclarationsWithoutLeakingAnnotations) {
        parse(R"cpp(
            UCLASS()class DUMMY_API First {};
            struct PlainAfterClass {};
            USTRUCT(
                BlueprintType, meta=(DisplayName="Dummy, (value)")
            ) /* gap */ struct [[nodiscard]] Second {};
            UINTERFACE() class Interface {};
            UENUM()enum Unscoped { Value };
            enum class PlainAfterEnum { Other };
            UENUM() enum class Scoped : unsigned { One };
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"First", "DummyModule"},
                                           {"PlainAfterClass", ""},
                                           {"Second", "DummyModule"},
                                           {"Interface", "DummyModule"},
                                           {"Unscoped", "DummyModule"},
                                           {"PlainAfterEnum", ""},
                                           {"Scoped", "DummyModule"}});
              });
    }

    TEST_F(ReflectionDbTest, NestedRecordsFieldsEnumsAndMethodsHaveIndependentAnnotations) {
        parse(R"cpp(
            UCLASS() struct Outer {
                GENERATED_BODY()
                int plain;
                UPROPERTY()int property;
                unsigned plainBits : 2;
                UPROPERTY() unsigned bits : 3;
                UPROPERTY() int initialized = 7;
                UPROPERTY() int array[2];
                void plainMethod();
                UFUNCTION()void method();
                USTRUCT() struct Nested {
                    int plain;
                    UPROPERTY() int property;
                    UFUNCTION() void method();
                };
                struct PlainNested {};
                UENUM() enum class InnerEnum { Value };
                enum PlainEnum { Other };
                int tail;
            };
            struct PlainAfter {};
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Outer", "DummyModule"},
                                           {"Outer::plain", ""},
                                           {"Outer::property", "DummyModule"},
                                           {"Outer::plainBits", ""},
                                           {"Outer::bits", "DummyModule"},
                                           {"Outer::initialized", "DummyModule"},
                                           {"Outer::array", "DummyModule"},
                                           {"Outer::plainMethod", ""},
                                           {"Outer::method", "DummyModule"},
                                           {"Outer::Nested", "DummyModule"},
                                           {"Outer::Nested::plain", ""},
                                           {"Outer::Nested::property", "DummyModule"},
                                           {"Outer::Nested::method", "DummyModule"},
                                           {"Outer::PlainNested", ""},
                                           {"Outer::InnerEnum", "DummyModule"},
                                           {"Outer::PlainEnum", ""},
                                           {"Outer::tail", ""},
                                           {"PlainAfter", ""}});
              });
    }

    TEST_F(ReflectionDbTest, MethodBodiesDoNotHideNestedReflectedDeclarations) {
        parse(R"cpp(
            struct Owner {
                UFUNCTION() void inlineMethod() {
                    USTRUCT() struct Local { UPROPERTY() int value; };
                }
                void plain() {}
                UFUNCTION() virtual void pure() = 0;
                UFUNCTION() void deleted() = delete;
                UFUNCTION() Owner() = default;
                UFUNCTION() int qualified(int value = 2) const noexcept { return value; }
                UFUNCTION() void external();
            };
            void Owner::external() { USTRUCT() struct Local {}; }
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Owner", ""}, {"Owner::inlineMethod", "DummyModule"}});
                  auto* method = find<clang::CXXMethodDecl>(context, "Owner::inlineMethod");
                  ASSERT_NE(method, nullptr);
                  // Local declarations belong to the method's DeclContext, not the record.
                  auto* local = llvm::dyn_cast<clang::DeclStmt>(*method->getBody()->child_begin());
                  ASSERT_NE(local, nullptr);
                  auto* record = llvm::dyn_cast<clang::RecordDecl>(local->getSingleDecl());
                  ASSERT_NE(record, nullptr);
                  EXPECT_EQ(ReflectionDb::registerReflectable(record), "DummyModule");
                  EXPECT_EQ(ReflectionDb::registerReflectable(*record->field_begin()), "DummyModule");
                  expectPackages(context, {{"Owner::plain", ""},
                                           {"Owner::pure", "DummyModule"},
                                           {"Owner::deleted", "DummyModule"},
                                           {"Owner::Owner", "DummyModule"},
                                           {"Owner::qualified", "DummyModule"},
                                           {"Owner::external", "DummyModule"}});
                  auto* definition = find<clang::CXXMethodDecl>(context, "Owner::external", 1);
                  ASSERT_NE(definition, nullptr);
                  EXPECT_TRUE(ReflectionDb::registerReflectable(definition).empty());
                  auto* statement = llvm::dyn_cast<clang::DeclStmt>(*definition->getBody()->child_begin());
                  ASSERT_NE(statement, nullptr);
                  EXPECT_EQ(ReflectionDb::registerReflectable(llvm::cast<clang::RecordDecl>(statement->getSingleDecl())), "DummyModule");
              });
    }

    TEST_F(ReflectionDbTest, ForwardDeclarationsRemainUnreflectedAfterTheirDefinitionsAreRegistered) {
        parse(R"cpp(
            struct Record;
            enum class Enum;
            USTRUCT() struct Record {};
            UENUM() enum class Enum { Value };
        )cpp",
              [](clang::ASTContext& context) {
                  auto* forward_record = find<clang::RecordDecl>(context, "Record");
                  auto* forward_enum   = find<clang::EnumDecl>(context, "Enum");
                  ASSERT_NE(forward_record, nullptr);
                  ASSERT_NE(forward_enum, nullptr);
                  EXPECT_TRUE(ReflectionDb::registerReflectable(forward_record).empty());
                  EXPECT_TRUE(ReflectionDb::registerReflectable(forward_enum).empty());
                  EXPECT_EQ(package(find(context, "Record", 1)), "DummyModule");
                  EXPECT_EQ(package(find(context, "Enum", 1)), "DummyModule");
                  EXPECT_TRUE(ReflectionDb::registerReflectable(forward_record).empty());
                  EXPECT_TRUE(ReflectionDb::registerReflectable(forward_enum).empty());
              });
    }

    TEST_F(ReflectionDbTest, LegacyEnumNamespaceSharesItsPackageOnlyWithItsSingleUnscopedEnum) {
        parse(R"cpp(
            UENUM() namespace Legacy { enum Type { A, B }; }
            namespace Legacy { enum Other { C }; }
            UENUM() namespace Empty {}
            UENUM() namespace Multiple { enum A { First }; enum B { Second }; }
            UENUM() namespace Record { struct Type {}; }
            UENUM() namespace Scoped { enum class Type { Value }; }
            UENUM() namespace Forward { enum Type : int; }
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Legacy", "DummyModule"}, {"Legacy::Type", "DummyModule"}});
                  EXPECT_TRUE(package(find(context, "Legacy", 1)).empty());
                  expectPackages(context, {{"Legacy::Other", ""}, {"Empty", ""}, {"Multiple", ""}, {"Record", ""}, {"Scoped", ""}, {"Forward", ""}});
              });
    }

    TEST_F(ReflectionDbTest, MacroExpandedAnnotationsUseTheInvocationLocation) {
        parse(R"cpp(
            #define REFLECTED_RECORD(...) USTRUCT(__VA_ARGS__)
            #define REFLECTED_FIELD(...) UPROPERTY(__VA_ARGS__)
            #define DECLARE_RECORD(Name) struct Name {}
            REFLECTED_RECORD(BlueprintType) struct Record {
                REFLECTED_FIELD(EditAnywhere) int field;
                int plain;
            };
            USTRUCT() DECLARE_RECORD(Expanded);
            struct Tail {};
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(
                      context, {{"Record", "DummyModule"}, {"Record::field", "DummyModule"}, {"Record::plain", ""}, {"Expanded", ""}, {"Tail", ""}});
                  // A whole declaration expanded from one invocation has a zero-width
                  // expansion range; ReflectionDb deliberately cannot associate it.
              });
    }

    TEST_F(ReflectionDbTest, IgnoredMacroDeclarationsDoNotDonateAnnotationsToTheFollowingDeclaration) {
        parse(R"cpp(
            #define DECLARE_RECORD(Name) struct Name {}
            #define DECLARE_ENUM(Name) enum class Name { Value }
            #define DECLARE_METHOD(Name) void Name()
            #define DECLARE_FIELD(Name) int Name
            USTRUCT() DECLARE_RECORD(GeneratedRecord);
            struct PlainRecord {};
            UENUM() DECLARE_ENUM(GeneratedEnum);
            enum class PlainEnum { Value };
            struct Owner {
                UFUNCTION() DECLARE_METHOD(generatedMethod);
                void plainMethod();
                UPROPERTY() DECLARE_FIELD(generatedField);
                int plainField;
            };
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"GeneratedRecord", ""},
                                           {"PlainRecord", ""},
                                           {"GeneratedEnum", ""},
                                           {"PlainEnum", ""},
                                           {"Owner", ""},
                                           {"Owner::generatedMethod", ""},
                                           {"Owner::plainMethod", ""},
                                           {"Owner::generatedField", ""},
                                           {"Owner::plainField", ""}});
              });
    }

    TEST_F(ReflectionDbTest, UnrelatedMacrosDoNotMaskReflectionAnnotations) {
        parse(R"cpp(
            UCLASS() UNRELATED(any, arguments) struct Record {
                UPROPERTY() UNRELATED() int field;
                UFUNCTION() UNRELATED() void method();
            };
            UENUM() UNRELATED() enum class Enum { Value };
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(
                      context,
                      {{"Record", "DummyModule"}, {"Record::field", "DummyModule"}, {"Record::method", "DummyModule"}, {"Enum", "DummyModule"}});
              });
    }

    TEST_F(ReflectionDbTest, RepeatedQueriesRetainPositiveAndNegativeResultsAfterRegisteringOtherDeclarations) {
        parse(R"cpp(
            USTRUCT() struct Record { UPROPERTY() int field; UFUNCTION() void method(); int plain; };
            UENUM() enum class Enum { Value };
            UENUM() namespace Legacy { enum Type { Value }; }
            struct Tail {};
        )cpp",
              [](clang::ASTContext& context) {
                  std::vector<std::pair<std::string, std::string>> expectations{
                      {"Record", "DummyModule"}, {"Record::field", "DummyModule"}, {"Record::method", "DummyModule"}, {"Record::plain", ""},
                      {"Enum", "DummyModule"},   {"Legacy", "DummyModule"},        {"Legacy::Type", "DummyModule"},   {"Tail", ""}};
                  expectPackages(context, expectations);
                  std::reverse(expectations.begin(), expectations.end());
                  expectPackages(context, expectations);
              });
    }

    TEST_F(ReflectionDbTest, LegacyNamespaceCanBeRegisteredAfterItsEnumWasQueried) {
        parse("UENUM() namespace Legacy { enum Type { Value }; }", [](clang::ASTContext& context) {
            expectPackages(context, {{"Legacy::Type", "DummyModule"}, {"Legacy", "DummyModule"}, {"Legacy::Type", "DummyModule"}});
        });
    }

    TEST_F(ReflectionDbTest, DelegateKindsAreNotMistakenForRecordAnnotations) {
        for (const auto kind : {ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE, ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE_MULTICAST,
                                ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE_MULTICAST_SPARSE}) {
            SCOPED_TRACE(static_cast<int>(kind));
            parse(
                "struct Target {};",
                [&](clang::ASTContext& context) {
                    add(context, kind, 0, 0);
                    EXPECT_THROW((void)package(find(context, "Target")), UEMeta::DeclException<>);
                },
                "Module/Delegate.cpp", false);
        }
    }

    TEST_F(ReflectionDbTest, NearestMacroWinsAndAFutureMacroCannotAnnotateAnEarlierDeclaration) {
        parse(R"cpp(
            struct Before {};
            UENUM() USTRUCT() struct Right {};
            struct Plain {};
            USTRUCT() UENUM() struct Wrong {};
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Before", ""}, {"Right", "DummyModule"}, {"Plain", ""}});
                  EXPECT_THROW((void)package(find(context, "Wrong")), UEMeta::DeclException<>);
              });
    }

    TEST_F(ReflectionDbTest, ManuallyRecordedMacrosAreSortedAndEndOffsetsAreExclusive) {
        parse(
            "struct First {}; struct Second {}; struct Third {};",
            [](clang::ASTContext& context) {
                auto* first  = find<clang::RecordDecl>(context, "First");
                auto* second = find<clang::RecordDecl>(context, "Second");
                auto* third  = find<clang::RecordDecl>(context, "Third");
                ASSERT_NE(first, nullptr);
                ASSERT_NE(second, nullptr);
                ASSERT_NE(third, nullptr);
                auto&      source       = context.getSourceManager();
                const auto second_begin = source.getFileOffset(second->getBeginLoc());
                const auto third_begin  = source.getFileOffset(third->getBeginLoc());
                add(context, ParserTypes::REFLECTION_KIND_UCLASS, third_begin - 1, third_begin);
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, second_begin - 1, second_begin);
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, second_begin - 1, second_begin); // Duplicate event.
                expectPackages(context, {{"First", ""}, {"Second", "DummyModule"}, {"Third", "DummyModule"}});
            },
            "Module/Public/Offsets.cpp", false);
    }

    TEST_F(ReflectionDbTest, MacroEndingAfterDeclarationBeginsDoesNotMatch) {
        parse(
            "struct Target {};",
            [](clang::ASTContext& context) {
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 1);
                expectPackages(context, {{"Target", ""}});
            },
            "Module/AfterBegin.cpp", false);
    }

    TEST_F(ReflectionDbTest, AConsumedMacroIsRejectedBeforeItsKindIsChecked) {
        parse("USTRUCT() struct Record {}; enum class Enum { Value };",
              [](clang::ASTContext& context) { expectPackages(context, {{"Record", "DummyModule"}, {"Enum", ""}}); });
        parse(
            "struct Record {}; enum class Enum { Value };",
            [](clang::ASTContext& context) {
                auto* record = find<clang::RecordDecl>(context, "Record");
                ASSERT_NE(record, nullptr);
                EXPECT_TRUE(ReflectionDb::registerReflectable(record).empty());
                const auto end = context.getSourceManager().getFileOffset(record->getBraceRange().getBegin());
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, end);
                // Equality must also count as consumed, avoiding an erroneous kind mismatch.
                expectPackages(context, {{"Enum", ""}});
            },
            "Module/Consumed.cpp", false);
    }

    TEST_F(ReflectionDbTest, ADeclarationRegisteredLaterInTheFileIsNotAnEarlierDeclarationsPredecessor) {
        parse("USTRUCT() struct First {}; UCLASS() struct Last {};",
              [](clang::ASTContext& context) { expectPackages(context, {{"Last", "DummyModule"}, {"First", "DummyModule"}}); });
    }

    TEST_F(ReflectionDbTest, EmptyResultsStayCachedAfterNewMacrosAreRecorded) {
        parse(
            "struct Target {};",
            [](clang::ASTContext& context) {
                auto* target = find<clang::RecordDecl>(context, "Target");
                ASSERT_NE(target, nullptr);
                EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 0);
                EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
                ReflectionDb::reset();
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 0);
                EXPECT_EQ(ReflectionDb::registerReflectable(target), "DummyModule");
            },
            "Module/Cached.cpp", false);
    }

    TEST_F(ReflectionDbTest, ResetClearsPositiveDeclarationMacroAndFilePackageCaches) {
        parse("USTRUCT() struct Target {};", [&](clang::ASTContext& context) {
            auto* target = find<clang::RecordDecl>(context, "Target");
            ASSERT_NE(target, nullptr);
            EXPECT_EQ(ReflectionDb::registerReflectable(target), "DummyModule");
            ReflectionDb::reset();
            EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
            ReflectionDb::reset();
            std::filesystem::rename(root / "Module/DummyModule.Build.cs", root / "Module/Renamed.Build.cs");
            add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, context.getSourceManager().getFileOffset(target->getBeginLoc()));
            EXPECT_EQ(ReflectionDb::registerReflectable(target), "Renamed");
        });
    }

    TEST_F(ReflectionDbTest, InvalidDegenerateAndCrossFileDeclarationRangesAreNotReflected) {
        parse("USTRUCT() struct Target {};", [](clang::ASTContext& context) {
            auto* target = find<clang::RecordDecl>(context, "Target");
            ASSERT_NE(target, nullptr);
            const auto begin  = target->getBeginLoc();
            const auto braces = target->getBraceRange();
            target->setLocStart({});
            EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
            ReflectionDb::reset();
            target->setLocStart(begin);
            target->setBraceRange({{}, braces.getEnd()});
            EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
            ReflectionDb::reset();
            target->setBraceRange({begin, braces.getEnd()});
            EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
            ReflectionDb::reset();
            auto&      source     = context.getSourceManager();
            const auto other_file = source.createFileID(llvm::MemoryBuffer::getMemBuffer("{}", "Other.cpp"));
            target->setBraceRange({source.getLocForStartOfFile(other_file), braces.getEnd()});
            EXPECT_TRUE(ReflectionDb::registerReflectable(target).empty());
            target->setBraceRange(braces);
        });
    }

    TEST_F(ReflectionDbTest, FilesWithoutRealPathsAreRejectedWhenExtensionsAreEnabled) {
        parse(
            "struct Target {};",
            [](clang::ASTContext& context) {
                auto& source = context.getSourceManager();
                EXPECT_THROW(ReflectionDb::addReflectionMacro({}, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 1, source), std::runtime_error);
                ReflectionDb::reset();
                const auto memory = source.createFileID(llvm::MemoryBuffer::getMemBuffer("struct Virtual {};", "memory.cpp"));
                EXPECT_THROW(add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 1, memory), std::runtime_error);
                ReflectionDb::reset();
                const auto file       = source.getFileManager().getVirtualFileRef("nonexistent-reflection-fixture.cpp", 0, 0);
                const auto virtual_id = source.createFileID(file, {}, clang::SrcMgr::C_User);
                EXPECT_THROW(add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 1, virtual_id), std::runtime_error);
            },
            "Module/Virtual.cpp", false);
    }

    TEST_F(ReflectionDbTest, PackageDiscoveryChoosesNearestBuildFileAndKeepsFilesIndependent) {
        write("Module/Nested/Inner.Build.cs", "");
        write("Module/Nested/Public/Inner.hpp", "USTRUCT() struct Inner {}; struct PlainInner {};\n");
        write("Other/Second.Build.cs", "");
        write("Other/Public/Other.hpp", "UENUM() enum class Other { Value };\n");
        write("Module/Public/Sibling.hpp", "UCLASS() struct Sibling {};\n");
        parse(R"cpp(
            #include "Sibling.hpp"
            #include "../Nested/Public/Inner.hpp"
            #include "../../Other/Public/Other.hpp"
            USTRUCT() struct Main {};
            struct PlainMain {};
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Sibling", "DummyModule"},
                                           {"Inner", "Inner"},
                                           {"PlainInner", ""},
                                           {"Other", "Second"},
                                           {"Main", "DummyModule"},
                                           {"PlainMain", ""}});
              });
    }

    TEST_F(ReflectionDbTest, PackageCachesHandleKnownFilesAndPreviouslyVisitedDirectories) {
        write("Module/Public/Known.hpp", "struct Known {};\n");
        write("Module/Public/Later.hpp", "USTRUCT() struct Later {};\n");
        write("Module/Root.hpp", "struct Root {};\n");
        write("Module/Deep/Branch/Nested.hpp", "USTRUCT() struct Nested {};\n");
        write("Module/Unparsed.txt", "not a source file");
        std::filesystem::create_directories(root / "Module/Directory.Build.cs");
        parse(R"cpp(
            #include "../Root.hpp"
            #include "Known.hpp"
            UCLASS() struct First {};
            #include "Later.hpp"
            #include "../Deep/Branch/Nested.hpp"
            USTRUCT() struct Last {};
        )cpp",
              [](clang::ASTContext& context) {
                  expectPackages(context, {{"Root", ""},
                                           {"Known", ""},
                                           {"First", "DummyModule"},
                                           {"Later", "DummyModule"},
                                           {"Nested", "DummyModule"},
                                           {"Last", "DummyModule"}});
              });
    }

    TEST_F(ReflectionDbTest, PackageNamesComeFromTheBuildFilenameIncludingDotsAndUnicode) {
        write("OddDirectory/Feature.Editor.Build.cs", "");
        parse(
            "USTRUCT() struct Target {};", [](clang::ASTContext& context) { expectPackages(context, {{"Target", "Feature.Editor"}}); },
            "OddDirectory/Public/Dotted.cpp");
        const auto unicode_name = std::filesystem::path{u8"M\u00f3dulo.Build.cs"};
        write(std::filesystem::path{"UnicodeDirectory"} / unicode_name, "");
        parse(
            "UCLASS() struct Target {};",
            [](clang::ASTContext& context) {
                expectPackages(context, {{"Target", "M\xc3\xb3"
                                                    "dulo"}});
            },
            "UnicodeDirectory/Unicode.cpp");
    }

    TEST_F(ReflectionDbTest, MissingPackagesThrowOnlyForMatchingAnnotationsAndSearchStopsAtDelimiter) {
        write("Outside.Build.cs", "// At the delimiter itself, outside the search scope.");
        write("NoPackage/.Build.cs", "");
        write("NoPackage/Wrong.build.cs", "");
        write("NoPackage/Wrong.Build.cs.bak", "");
        std::filesystem::create_directories(root / "NoPackage/Directory.Build.cs");
        parse(
            "struct Plain {}; USTRUCT() struct Missing {};",
            [](clang::ASTContext& context) {
                expectPackages(context, {{"Plain", ""}});
                auto* missing = find(context, "Missing");
                ASSERT_NE(missing, nullptr);
                try {
                    (void)package(missing);
                    FAIL() << "A reflected declaration without a package must throw";
                }
                catch (const UEMeta::DeclException<>& error) {
                    EXPECT_NE(std::string{error.what()}.find("Failed to find unreal package"), std::string::npos);
                }
            },
            "NoPackage/Private/Missing.cpp");
    }

    TEST_F(ReflectionDbTest, FailedPackageDiscoveryIsRetriedWhenAMarkerBecomesAvailable) {
        parse(
            "struct First {}; struct Second {};",
            [&](clang::ASTContext& context) {
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 0);
                auto* first = find(context, "First");
                ASSERT_NE(first, nullptr);
                EXPECT_THROW((void)package(first), UEMeta::DeclException<>);
                write("NewModule/Discovered.Build.cs", "");
                auto* second = find(context, "Second");
                ASSERT_NE(second, nullptr);
                const auto begin = context.getSourceManager().getFileOffset(second->getBeginLoc());
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, begin - 1, begin);
                EXPECT_EQ(package(first), "Discovered");
                EXPECT_EQ(package(second), "Discovered");
            },
            "NewModule/Public/Retry.cpp", false);
    }

    TEST_F(ReflectionDbTest, UnavailableSiblingFilesDoNotPreventPackageDiscovery) {
        parse(
            "struct Target {};",
            [&](clang::ASTContext& context) {
                // The directory entry exists, but Clang cannot access it. Inject
                // that failure deterministically instead of racing a file deletion.
                write("Module/Public/Unavailable.txt", "Not visible to Clang.");
                class UnavailableFileSystem final : public llvm::vfs::ProxyFileSystem {
                public:
                    using ProxyFileSystem::ProxyFileSystem;
                    unsigned                         failures = 0;
                    llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine& path) override {
                        if (std::filesystem::path{path.str()}.filename() == "Unavailable.txt") {
                            ++failures;
                            return std::make_error_code(std::errc::permission_denied);
                        }
                        return ProxyFileSystem::status(path);
                    }
                };
                auto& files      = context.getSourceManager().getFileManager();
                auto  filesystem = llvm::makeIntrusiveRefCnt<UnavailableFileSystem>(files.getVirtualFileSystemPtr());
                files.setVirtualFileSystem(filesystem);
                add(context, ParserTypes::REFLECTION_KIND_USTRUCT, 0, 0);
                expectPackages(context, {{"Target", "DummyModule"}});
                EXPECT_GT(filesystem->failures, 0u);
            },
            "Module/Public/CacheMiss.cpp", false);
    }

    TEST_F(ReflectionDbTest, DelayedTemplateMethodsCanBeReflectedBeforeTheirBodiesAreParsed) {
        parse(R"cpp(
            struct Owner {
                UFUNCTION() template<class T> void method(T value) { (void)value; }
            };
        )cpp",
              [](clang::ASTContext& context) {
                  auto* owner = find<clang::RecordDecl>(context, "Owner");
                  ASSERT_NE(owner, nullptr);
                  clang::CXXMethodDecl* method = nullptr;
                  for (auto* declaration : owner->decls()) {
                      if (auto* function = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration))
                          method = llvm::cast<clang::CXXMethodDecl>(function->getTemplatedDecl());
                  }
                  ASSERT_NE(method, nullptr);
                  ASSERT_TRUE(method->isLateTemplateParsed());
                  ASSERT_TRUE(method->doesThisDeclarationHaveABody());
                  ASSERT_EQ(method->getBody(), nullptr);
                  EXPECT_EQ(ReflectionDb::registerReflectable(method), "DummyModule");
                  EXPECT_EQ(ReflectionDb::registerReflectable(method), "DummyModule");
              },
              "Module/Delayed.cpp", true, {"-std=c++17", "-fdelayed-template-parsing"});
    }

    TEST_F(ReflectionDbTest, PackageViewsRemainValidWhenManyPackageRootsAreInserted) {
        std::string        includes;
        constexpr unsigned count = 48;
        for (unsigned i = 0; i < count; ++i) {
            const auto name = "Package" + std::to_string(i);
            write(name + "/" + name + ".Build.cs", "");
            write(name + "/Public/Record.hpp", "USTRUCT() struct Record" + std::to_string(i) + " {};\n");
            includes += "#include \"../../" + name + "/Public/Record.hpp\"\n";
        }
        parse(includes, [](clang::ASTContext& context) {
            for (unsigned i = 0; i < count; ++i) {
                SCOPED_TRACE(i);
                auto* record = find(context, "Record" + std::to_string(i));
                ASSERT_NE(record, nullptr);
                EXPECT_EQ(package(record), "Package" + std::to_string(i));
            }
        });
    }
} // namespace
