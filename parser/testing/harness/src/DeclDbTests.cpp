#include <fstream>
#include <iterator>
#include <type_traits>

#include "ProtoAssertions.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "WrapperTest.hpp"
#include "clang/AST/DeclTemplate.h"
#include "google/protobuf/util/json_util.h"

namespace {
    using namespace UEMeta::Testing;
    using UEMeta::Config;
    using UEMeta::DeclDb;
    using UEMeta::Hash;
    using Format = Config::SerializationFormat;

    // Only the forward-file tests change format. Wrapper file writers cache their
    // format, so those tests keep the process's normal binary configuration.
    class ScopedSerializationFormat {
    public:
        explicit ScopedSerializationFormat(Config::SerializationFormat format) : previous(Config::getConfig().getFormat()) {
            Config::getConfig().setFormatForTesting(format);
        }
        ~ScopedSerializationFormat() { Config::getConfig().setFormatForTesting(previous); }
        ScopedSerializationFormat(const ScopedSerializationFormat&)            = delete;
        ScopedSerializationFormat& operator=(const ScopedSerializationFormat&) = delete;

    private:
        Config::SerializationFormat previous;
    };

    Hash identity(uint64_t a, uint64_t b) {
        Hash result{};
        result.a = a;
        result.b = b;
        return result;
    }

    template <typename T>
    std::vector<T*> declarations(clang::DeclContext* context, llvm::StringRef name = {}) {
        std::vector<T*> result;
        for (auto* declaration : context->decls()) {
            if (auto* candidate = llvm::dyn_cast<T>(declaration);
                candidate && !candidate->isImplicit() && (name.empty() || candidate->getName() == name))
                result.push_back(candidate);
        }
        return result;
    }

    template <typename T>
    void expectQuery(const DeclDb::QueryResult& actual, const T& expected) {
        ASSERT_TRUE(std::holds_alternative<T>(actual)) << "Unexpected QueryResult alternative: " << actual.index();
        EXPECT_EQ(std::get<T>(actual), expected);
    }

    class DeclDbTest : public WrapperTest {
    protected:
        static std::filesystem::path forwardPath() {
            return Config::getConfig().getOutputDirectory().getUnderlyingPath() /
                   (Config::getConfig().getFormat() == Format::Json ? "fwd.decljson" : "fwd.declbin");
        }

        static ParserTypes::ForwardDeclarationList readForwardFile() {
            ParserTypes::ForwardDeclarationList result;
            std::ifstream                       input{forwardPath(), std::ios::binary};
            EXPECT_TRUE(input.is_open()) << forwardPath();
            if (Config::getConfig().getFormat() == Format::Json) {
                const std::string json{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
                const auto        status = google::protobuf::util::JsonStringToMessage(json, &result);
                EXPECT_TRUE(status.ok()) << status.ToString();
            }
            else {
                EXPECT_TRUE(result.ParseFromIstream(&input));
            }
            return result;
        }

        static ParserTypes::ForwardDeclarationList forwardList(std::string_view declarations = {}) {
            auto expected = proto<ParserTypes::ForwardDeclarationList>(declarations);
            expected.set_version("test-version");
            return expected;
        }

        // DenseMap iteration order is unspecified; order within each occurrence
        // list is part of the file contract and must remain significant.
        static void expectForwards(const ParserTypes::ForwardDeclarationList& actual, const ParserTypes::ForwardDeclarationList& expected) {
            google::protobuf::util::MessageDifferencer differencer;
            differencer.TreatAsSet(actual.GetDescriptor()->FindFieldByName("forward_declarations"));
            std::string differences;
            differencer.ReportDifferencesToString(&differences);
            EXPECT_TRUE(differencer.Compare(expected, actual)) << differences;
        }
    };

    TEST_F(DeclDbTest, NullInputsAndUnknownIdentitiesDoNotCreateState) {
        expectQuery(DeclDb::queryDeclIdentity(nullptr), false);
        EXPECT_EQ(DeclDb::queryDecl(Hash{}), nullptr);
        EXPECT_EQ(DeclDb::queryDecl(identity(1, 2)), nullptr);
        EXPECT_THROW(DeclDb::addDeclIdentity(nullptr, identity(1, 2)), UEMeta::DeclException<>);
        EXPECT_THROW(DeclDb::addDeclarationAsVisited(nullptr), UEMeta::DeclException<>);
        DeclDb::serializeIfNeeded(static_cast<clang::RecordDecl*>(nullptr));
        DeclDb::serializeIfNeeded(static_cast<clang::EnumDecl*>(nullptr));
        DeclDb::serializeIfNeeded(static_cast<clang::FunctionDecl*>(nullptr));
        DeclDb::serializeIfNeeded(static_cast<clang::VarDecl*>(nullptr));
        DeclDb::serializeIfNeeded(static_cast<clang::NamespaceDecl*>(nullptr));
        auto* context = parseCode("struct Unknown {};");
        ASSERT_NE(context, nullptr);
        const auto records = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        expectQuery(DeclDb::queryDeclIdentity(records[0]), false);
        clang::QualType unwrapped = context->IntTy;
        expectQuery(DeclDb::queryType({}, &unwrapped), false);
        EXPECT_TRUE(unwrapped.isNull());
        EXPECT_TRUE(outputFiles().empty());
    }

    TEST_F(DeclDbTest, RegisteredIdentitySupportsReverseLookupAndPreventsSerialization) {
        auto* context = parseCode("struct Known {};");
        ASSERT_NE(context, nullptr);
        const auto records = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        const auto id = identity(17, 29);
        DeclDb::addDeclIdentity(records[0], id);
        DeclDb::addDeclIdentity(records[0], id);
        expectQuery(DeclDb::queryDeclIdentity(records[0]), id);
        EXPECT_EQ(DeclDb::queryDecl(id), records[0]);
        DeclDb::serializeIfNeeded(records[0]);
        EXPECT_TRUE(outputFiles().empty());
    }

    TEST_F(DeclDbTest, RejectsForwardOccurrencesForNullNonDefinitionsAndOtherDeclarationKinds) {
        auto* context = parseCode("struct Record; enum class Enum; void function(); int variable; namespace N {}");
        ASSERT_NE(context, nullptr);
        EXPECT_THROW(DeclDb::addForwardDeclaration(nullptr), UEMeta::DeclException<>);
        unsigned checked = 0;
        for (auto* declaration : context->getTranslationUnitDecl()->decls()) {
            if (declaration->isImplicit())
                continue;
            EXPECT_THROW(DeclDb::addForwardDeclaration(declaration), UEMeta::DeclException<>);
            ++checked;
        }
        EXPECT_EQ(checked, 5u);
        // Rejections must not consume occurrence indices.
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 0u);
    }

    TEST_F(DeclDbTest, LatestForwardOccurrenceYieldsToDefinitionIdentity) {
        auto* context = parseCode("struct Record {}; enum class Enum {}; void function(); void function() {}");
        ASSERT_NE(context, nullptr);
        const auto records   = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        const auto enums     = declarations<clang::EnumDecl>(context->getTranslationUnitDecl());
        const auto functions = declarations<clang::FunctionDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(enums.size(), 1u);
        ASSERT_EQ(functions.size(), 2u);
        const std::vector<clang::Decl*> definitions{records[0], enums[0], functions[1]};
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            DeclDb::addForwardDeclaration(definitions[i]);
            expectQuery(DeclDb::queryDeclIdentity(definitions[i]), uint64_t{2 * i});
            DeclDb::addForwardDeclaration(definitions[i]);
            expectQuery(DeclDb::queryDeclIdentity(definitions[i]), uint64_t{2 * i + 1});
        }
        expectQuery(DeclDb::queryDeclIdentity(functions[0]), uint64_t{5});
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            const auto id = identity(i + 1, i + 10);
            DeclDb::addDeclIdentity(definitions[i], id);
            DeclDb::addForwardDeclaration(definitions[i]);
            expectQuery(DeclDb::queryDeclIdentity(definitions[i]), id);
        }
        expectQuery(DeclDb::queryDeclIdentity(functions[0]), identity(3, 12));
        EXPECT_TRUE(outputFiles().empty());
    }

    TEST_F(DeclDbTest, ResetClearsIdentitiesForwardHistoryAndVisitedDeclarations) {
        auto* context = parseCode("struct Registered {}; struct Forward {}; struct Visited {};");
        ASSERT_NE(context, nullptr);
        const auto records = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 3u);
        const auto id = identity(7, 9);
        DeclDb::addDeclIdentity(records[0], id);
        DeclDb::addForwardDeclaration(records[1]);
        DeclDb::addDeclarationAsVisited(records[2]);
        DeclDb::serializeIfNeeded(records[2]);
        EXPECT_TRUE(outputFiles().empty());
        DeclDb::reset();
        EXPECT_EQ(DeclDb::queryDecl(id), nullptr);
        for (auto* record : records) {
            expectQuery(DeclDb::queryDeclIdentity(record), false);
            DeclDb::serializeIfNeeded(record);
            EXPECT_TRUE(std::holds_alternative<Hash>(DeclDb::queryDeclIdentity(record)));
        }
        EXPECT_EQ(outputFiles().size(), 3u);
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList());
    }

    template <typename T>
    class DeclDbRedeclarationTest : public DeclDbTest {
    protected:
        using Message = std::conditional_t<
            std::is_same_v<T, clang::RecordDecl>, ParserTypes::TLRecordDeclaration,
            std::conditional_t<std::is_same_v<T, clang::EnumDecl>, ParserTypes::TLEnumDeclaration, ParserTypes::TLFreeFunctionDeclaration>>;

        static std::string_view source() {
            if constexpr (std::is_same_v<T, clang::RecordDecl>)
                return "struct Target; struct Target; struct Target {}; struct Target;";
            else if constexpr (std::is_same_v<T, clang::EnumDecl>)
                return "enum class Target; enum class Target; enum class Target {}; enum class Target;";
            else
                return "void Target(); void Target(); void Target() {} void Target();";
        }
    };

    using ForwardKinds = ::testing::Types<clang::RecordDecl, clang::EnumDecl, clang::FunctionDecl>;
    TYPED_TEST_SUITE(DeclDbRedeclarationTest, ForwardKinds);

    TYPED_TEST(DeclDbRedeclarationTest, SerializationJoinsRedeclarationsToOneDefinitionInVisitationOrder) {
        auto* context = this->parseCode(this->source());
        ASSERT_NE(context, nullptr);
        const auto decls = declarations<TypeParam>(context->getTranslationUnitDecl());
        ASSERT_EQ(decls.size(), 4u);
        DeclDb::serializeIfNeeded(decls[0]);
        DeclDb::serializeIfNeeded(decls[0]); // Re-visiting the same AST node is a no-op.
        DeclDb::serializeIfNeeded(decls[1]);
        expectQuery(DeclDb::queryDeclIdentity(decls[2]), uint64_t{1});
        EXPECT_TRUE(this->outputFiles().empty());
        DeclDb::serializeIfNeeded(decls[2]);
        DeclDb::serializeIfNeeded(decls[2]);
        DeclDb::serializeIfNeeded(decls[3]); // A written redeclaration after the definition still counts.
        const auto id = enumId("::Target");  // All three no-parameter declarations hash this name.
        expectQuery(DeclDb::queryDeclIdentity(decls[2]), id);
        EXPECT_EQ(DeclDb::queryDecl(id), decls[2]);
        const auto files = this->outputFiles();
        ASSERT_EQ(files.size(), 1u);
        typename TestFixture::Message definition;
        std::ifstream                 input{files[0], std::ios::binary};
        ASSERT_TRUE(definition.ParseFromIstream(&input));
        expectProto(definition.metadata(), metadata("::Target", id, 2));
        EXPECT_EQ(files[0], this->outputPath(definition.metadata(), std::string{UEMeta::TOP_LEVEL_EXT<typename TestFixture::Message>} + "bin"));
        DeclDb::serializeForwardDeclarations();
        auto expected = this->forwardList(R"pb(
            forward_declarations { occurrence_indices: [0, 1, 3] }
        )pb");
        expected.mutable_forward_declarations(0)->mutable_type_id()->set_a(id.a);
        expected.mutable_forward_declarations(0)->mutable_type_id()->set_b(id.b);
        this->expectForwards(this->readForwardFile(), expected);
    }

    TEST_F(DeclDbTest, IncompleteTagsAreSkippedButExternalFunctionsAndVariablesProduceMetadata) {
        auto* context = parseCode("struct Missing; enum class Opaque; void external(); extern int variable;");
        ASSERT_NE(context, nullptr);
        const auto records   = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        const auto enums     = declarations<clang::EnumDecl>(context->getTranslationUnitDecl());
        const auto functions = declarations<clang::FunctionDecl>(context->getTranslationUnitDecl());
        const auto variables = declarations<clang::VarDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(enums.size(), 1u);
        ASSERT_EQ(functions.size(), 1u);
        ASSERT_EQ(variables.size(), 1u);
        DeclDb::serializeIfNeeded(records[0]);
        DeclDb::serializeIfNeeded(enums[0]);
        expectQuery(DeclDb::queryDeclIdentity(records[0]), false);
        expectQuery(DeclDb::queryDeclIdentity(enums[0]), false);
        EXPECT_TRUE(outputFiles().empty());
        DeclDb::serializeIfNeeded(functions[0]);
        DeclDb::serializeIfNeeded(variables[0]);
        DeclDb::serializeIfNeeded(functions[0]);
        DeclDb::serializeIfNeeded(variables[0]);
        EXPECT_EQ(outputFiles().size(), 2u);
        const auto                             function_metadata = metadata("::external", enumId("::external"), 0);
        const auto                             variable_metadata = metadata("::variable", variableId("::variable"), 1);
        ParserTypes::TLFreeFunctionDeclaration function;
        std::ifstream                          function_file{outputPath(function_metadata, "functionbin"), std::ios::binary};
        ASSERT_TRUE(function_file.is_open());
        ASSERT_TRUE(function.ParseFromIstream(&function_file));
        expectProto(function.metadata(), function_metadata);
        EXPECT_FALSE(function.common().has_inline_definition());
        ParserTypes::TLGlobalVariableDeclaration variable;
        std::ifstream                            variable_file{outputPath(variable_metadata, "varbin"), std::ios::binary};
        ASSERT_TRUE(variable_file.is_open());
        ASSERT_TRUE(variable.ParseFromIstream(&variable_file));
        expectProto(variable.metadata(), variable_metadata);
        expectVersioned(variable.storage_class(), ParserTypes::VAR_STORAGE_CLASS_EXTERN);
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList());
    }

    TEST_F(DeclDbTest, SystemAndStdDeclarationsAreHeaderReferencesAndDoNotSerialize) {
        const std::vector<std::string> sources{"namespace std { struct Record {}; enum Enum { Value }; void function() {} int variable; }",
                                               "# 1 \"library.hpp\" 3\nstruct Record {}; enum Enum { Value }; void function() {} int variable;"};
        for (const auto& source : sources) {
            auto* context = parseCode(source, "vendor/library.hpp");
            ASSERT_NE(context, nullptr);
            clang::DeclContext* scope      = context->getTranslationUnitDecl();
            const auto          namespaces = declarations<clang::NamespaceDecl>(scope);
            if (!namespaces.empty())
                scope = namespaces[0];
            const auto records   = declarations<clang::RecordDecl>(scope);
            const auto enums     = declarations<clang::EnumDecl>(scope);
            const auto functions = declarations<clang::FunctionDecl>(scope);
            const auto variables = declarations<clang::VarDecl>(scope);
            ASSERT_EQ(records.size(), 1u);
            ASSERT_EQ(enums.size(), 1u);
            ASSERT_EQ(functions.size(), 1u);
            ASSERT_EQ(variables.size(), 1u);
            DeclDb::serializeIfNeeded(records[0]);
            DeclDb::serializeIfNeeded(enums[0]);
            DeclDb::serializeIfNeeded(functions[0]);
            DeclDb::serializeIfNeeded(variables[0]);
            for (const clang::Decl* decl : std::vector<clang::Decl*>{records[0], enums[0], functions[0], variables[0]})
                expectQuery(DeclDb::queryDeclIdentity(decl), llvm::StringRef{"library.hpp"});
            const auto type = context->getCanonicalTagType(records[0]);
            DeclDb::addForwardDeclaration(records[0]);
            expectQuery(DeclDb::queryType(type), llvm::StringRef{"library.hpp"});
            DeclDb::addDeclIdentity(records[0], identity(13, 17));
            expectQuery(DeclDb::queryType(type), identity(13, 17));
        }
        EXPECT_TRUE(outputFiles().empty());
    }

    TEST_F(DeclDbTest, FunctionLocalDeclarationsAndClassMembersDoNotProduceTopLevelFiles) {
        auto* context = parseCode(R"cpp(
            struct Owner { static int member; void method(); };
            void function(int parameter) {
                struct Local { struct Nested {}; enum Kind { Value }; };
                enum LocalEnum { Item };
                static int local;
            }
        )cpp");
        ASSERT_NE(context, nullptr);
        const auto owners    = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        const auto functions = declarations<clang::FunctionDecl>(context->getTranslationUnitDecl(), "function");
        ASSERT_EQ(owners.size(), 1u);
        ASSERT_EQ(functions.size(), 1u);
        unsigned   checked = 0;
        const auto skip    = [&](this auto self, clang::DeclContext* scope) -> void {
            for (auto* decl : scope->decls()) {
                SCOPED_TRACE(llvm::cast<clang::NamedDecl>(decl)->getNameAsString());
                if (auto* record = llvm::dyn_cast<clang::RecordDecl>(decl)) {
                    DeclDb::serializeIfNeeded(record);
                    if (!record->isImplicit()) {
                        ++checked;
                        self(record);
                    }
                }
                else if (auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(decl)) {
                    DeclDb::serializeIfNeeded(enumeration);
                    ++checked;
                }
                else if (auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
                    DeclDb::serializeIfNeeded(function);
                    ++checked;
                }
                else if (auto* variable = llvm::dyn_cast<clang::VarDecl>(decl)) {
                    DeclDb::serializeIfNeeded(variable);
                    ++checked;
                }
                expectQuery(DeclDb::queryDeclIdentity(decl), false);
            }
        };
        skip(owners[0]);
        skip(functions[0]);
        EXPECT_EQ(checked, 8u); // Two members, parameter, two records, two enums and local variable.
        EXPECT_TRUE(outputFiles().empty());
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 0u);
    }

    TEST_F(DeclDbTest, ExternalFunctionDeclaredInABlockRetainsItsNamespaceIdentity) {
        auto* context = parseCode("void owner() { extern void external(); }");
        ASSERT_NE(context, nullptr);
        const auto owners = declarations<clang::FunctionDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(owners.size(), 1u);
        const auto functions = declarations<clang::FunctionDecl>(owners[0]);
        ASSERT_EQ(functions.size(), 1u);
        ASSERT_EQ(functions[0]->getLexicalDeclContext(), owners[0]);
        ASSERT_EQ(functions[0]->getDeclContext(), context->getTranslationUnitDecl());
        DeclDb::serializeIfNeeded(functions[0]);
        expectQuery(DeclDb::queryDeclIdentity(functions[0]), enumId("::external"));
        const auto files = outputFiles();
        ASSERT_EQ(files.size(), 1u);
        ParserTypes::TLFreeFunctionDeclaration function;
        std::ifstream                          input{files[0], std::ios::binary};
        ASSERT_TRUE(function.ParseFromIstream(&input));
        expectProto(function.metadata(), metadata("::external", enumId("::external"), 0));
    }

    TEST_F(DeclDbTest, ImplicitRecordEnumFunctionAndVariableInstantiationsDoNotSerialize) {
        auto* context = parseCode(R"cpp(
            template<class T> struct Box { enum class Kind { Value }; };
            template<class T> T function(T value) { return value; }
            template<class T> int variable = 0;
            Box<int> box;
            Box<int>::Kind kind = Box<int>::Kind::Value;
            int result = function(1) + variable<int>;
        )cpp");
        ASSERT_NE(context, nullptr);
        const auto boxes     = declarations<clang::VarDecl>(context->getTranslationUnitDecl(), "box");
        const auto kinds     = declarations<clang::VarDecl>(context->getTranslationUnitDecl(), "kind");
        const auto functions = declarations<clang::FunctionTemplateDecl>(context->getTranslationUnitDecl());
        const auto templates = declarations<clang::VarTemplateDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(boxes.size(), 1u);
        ASSERT_EQ(kinds.size(), 1u);
        ASSERT_EQ(functions.size(), 1u);
        ASSERT_EQ(templates.size(), 1u);
        auto* record      = boxes[0]->getType()->getAsCXXRecordDecl();
        auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(kinds[0]->getType()->getAsTagDecl());
        ASSERT_NE(record, nullptr);
        ASSERT_NE(enumeration, nullptr);
        ASSERT_EQ(record->getTemplateSpecializationKind(), clang::TSK_ImplicitInstantiation);
        ASSERT_EQ(enumeration->getTemplateSpecializationKind(), clang::TSK_ImplicitInstantiation);
        ASSERT_EQ(std::distance(functions[0]->spec_begin(), functions[0]->spec_end()), 1);
        ASSERT_EQ(std::distance(templates[0]->spec_begin(), templates[0]->spec_end()), 1);
        auto* function = *functions[0]->spec_begin();
        auto* variable = *templates[0]->spec_begin();
        ASSERT_EQ(function->getTemplateSpecializationKind(), clang::TSK_ImplicitInstantiation);
        ASSERT_EQ(variable->getTemplateSpecializationKind(), clang::TSK_ImplicitInstantiation);
        DeclDb::serializeIfNeeded(static_cast<clang::RecordDecl*>(record));
        DeclDb::serializeIfNeeded(enumeration);
        DeclDb::serializeIfNeeded(function);
        DeclDb::serializeIfNeeded(variable);
        for (const auto* decl : std::vector<clang::Decl*>{record, enumeration, function, variable})
            expectQuery(DeclDb::queryDeclIdentity(decl), false);
        EXPECT_TRUE(outputFiles().empty());
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 0u);
    }

    TEST_F(DeclDbTest, NamespaceSerializationIsDisabledWithoutUnrealExtensions) {
        auto* context = parseCode("namespace N { enum Kind { Value }; }");
        ASSERT_NE(context, nullptr);
        const auto namespaces = declarations<clang::NamespaceDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(namespaces.size(), 1u);
        ASSERT_FALSE(Config::getConfig().unrealExtensionsEnabled());
        DeclDb::serializeIfNeeded(namespaces[0]);
        EXPECT_TRUE(outputFiles().empty());
        expectQuery(DeclDb::queryDeclIdentity(namespaces[0]), false);
    }

    TEST_F(DeclDbTest, TypeQueriesReturnTheCanonicalUnwrappedTypeWithoutCreatingState) {
        auto* context = parseCode(R"cpp(
            struct Node {};
            using Alias = const Node;
            extern Alias* const (&nodes)[3];
            extern int (&numbers)[2];
            void (*callback)();
            int Node::* member;
            template<class T> struct Dependent { T value; typename T::type nested; };
        )cpp");
        ASSERT_NE(context, nullptr);
        const auto records   = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        const auto variables = declarations<clang::VarDecl>(context->getTranslationUnitDecl());
        const auto templates = declarations<clang::ClassTemplateDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(variables.size(), 4u);
        ASSERT_EQ(templates.size(), 1u);
        clang::QualType unwrapped;
        expectQuery(DeclDb::queryType(variables[0]->getType(), &unwrapped), false);
        EXPECT_TRUE(context->hasSameType(unwrapped, context->getCanonicalTagType(records[0]).withConst()));
        DeclDb::addForwardDeclaration(records[0]);
        expectQuery(DeclDb::queryType(variables[0]->getType()), uint64_t{0});
        DeclDb::addDeclIdentity(records[0], identity(11, 12));
        expectQuery(DeclDb::queryType(variables[0]->getType()), identity(11, 12));
        expectQuery(DeclDb::queryType(variables[1]->getType(), &unwrapped), true);
        EXPECT_EQ(unwrapped, context->IntTy);
        expectQuery(DeclDb::queryType(variables[2]->getType(), &unwrapped), false);
        EXPECT_TRUE(unwrapped->isFunctionType());
        expectQuery(DeclDb::queryType(variables[3]->getType(), &unwrapped), false);
        EXPECT_TRUE(unwrapped->isMemberPointerType());
        const auto fields = declarations<clang::FieldDecl>(templates[0]->getTemplatedDecl());
        ASSERT_EQ(fields.size(), 2u);
        for (auto* field : fields)
            expectQuery(DeclDb::queryType(field->getType()), true);
        const auto dependent_type = context->getCanonicalTagType(templates[0]->getTemplatedDecl());
        expectQuery(DeclDb::queryType(dependent_type), true);
        DeclDb::addDeclIdentity(templates[0]->getTemplatedDecl(), identity(21, 22));
        expectQuery(DeclDb::queryType(dependent_type), identity(21, 22));
        EXPECT_TRUE(outputFiles().empty());
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 1u);
    }

    TEST_F(DeclDbTest, InstantiatedTypesUseTheSelectedSourcePatternEvenWhenGeneratedIdentitiesExist) {
        auto* context = parseCode(R"cpp(
            template<class T> struct Box {};
            template<class T> struct Box<T*> {};
            template<> struct Box<char> {};
            template struct Box<long>;
            extern template struct Box<short>;
            Box<int> primary;
            Box<int*> partial;
            Box<char> explicit_specialization;
            Box<long> explicit_instantiation;
            Box<short> extern_instantiation;
        )cpp");
        ASSERT_NE(context, nullptr);
        const auto templates = declarations<clang::ClassTemplateDecl>(context->getTranslationUnitDecl());
        const auto partials  = declarations<clang::ClassTemplatePartialSpecializationDecl>(context->getTranslationUnitDecl());
        const auto variables = declarations<clang::VarDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(templates.size(), 1u);
        ASSERT_EQ(partials.size(), 1u);
        ASSERT_EQ(variables.size(), 5u);
        const std::vector<clang::CXXRecordDecl*> targets{templates[0]->getTemplatedDecl(), partials[0], variables[2]->getType()->getAsCXXRecordDecl(),
                                                         templates[0]->getTemplatedDecl(), templates[0]->getTemplatedDecl()};
        for (std::size_t i : {0u, 1u, 3u, 4u}) {
            SCOPED_TRACE(variables[i]->getNameAsString());
            auto* generated = variables[i]->getType()->getAsCXXRecordDecl();
            ASSERT_NE(generated, nullptr);
            ASSERT_EQ(generated->getTemplateInstantiationPattern(), targets[i]);
            DeclDb::addDeclIdentity(generated, identity(100 + i, 200 + i));
            // A generated identity cannot stand in for an unknown source pattern.
            expectQuery(DeclDb::queryType(variables[i]->getType()), false);
        }
        const std::vector<Hash> ids{identity(1, 2), identity(3, 4), identity(5, 6), identity(1, 2), identity(1, 2)};
        for (std::size_t i = 0; i < 3; ++i)
            DeclDb::addDeclIdentity(targets[i], ids[i]);
        for (std::size_t i = 0; i < variables.size(); ++i) {
            SCOPED_TRACE(variables[i]->getNameAsString());
            clang::QualType unwrapped;
            expectQuery(DeclDb::queryType(variables[i]->getType(), &unwrapped), ids[i]);
            EXPECT_EQ(unwrapped, variables[i]->getType().getCanonicalType());
        }
        EXPECT_TRUE(outputFiles().empty());
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 0u);
    }

    TEST_F(DeclDbTest, UninstantiatedSpecializationDoesNotGuessAPatternOrInstantiateIt) {
        auto* context = parseCode("template<class T> struct Box {}; Box<int>* pending;");
        ASSERT_NE(context, nullptr);
        const auto templates = declarations<clang::ClassTemplateDecl>(context->getTranslationUnitDecl());
        const auto variables = declarations<clang::VarDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(templates.size(), 1u);
        ASSERT_EQ(variables.size(), 1u);
        auto* generated = variables[0]->getType()->getPointeeType()->getAsCXXRecordDecl();
        ASSERT_NE(generated, nullptr);
        ASSERT_EQ(generated->getDefinition(), nullptr);
        ASSERT_EQ(generated->getTemplateInstantiationPattern(), nullptr);
        DeclDb::addDeclIdentity(templates[0]->getTemplatedDecl(), identity(1, 2));
        DeclDb::addDeclIdentity(generated, identity(3, 4));
        expectQuery(DeclDb::queryType(variables[0]->getType()), false);
        EXPECT_EQ(generated->getDefinition(), nullptr);
        EXPECT_TRUE(outputFiles().empty());
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 0u);
    }

    TEST_F(DeclDbTest, InstantiatedMemberEnumUsesItsSourceEnumsForwardOccurrenceAndIdentity) {
        auto* context = parseCode(R"cpp(
            template<class T> struct Box { enum class Kind { Value }; };
            Box<int>::Kind kind = Box<int>::Kind::Value;
        )cpp");
        ASSERT_NE(context, nullptr);
        const auto templates = declarations<clang::ClassTemplateDecl>(context->getTranslationUnitDecl());
        const auto variables = declarations<clang::VarDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(templates.size(), 1u);
        ASSERT_EQ(variables.size(), 1u);
        const auto enums = declarations<clang::EnumDecl>(templates[0]->getTemplatedDecl());
        ASSERT_EQ(enums.size(), 1u);
        const auto type      = variables[0]->getType();
        auto*      generated = llvm::dyn_cast<clang::EnumDecl>(type->getAsTagDecl());
        ASSERT_NE(generated, nullptr);
        ASSERT_EQ(generated->getTemplateInstantiationPattern(), enums[0]);
        DeclDb::addDeclIdentity(generated, identity(101, 102));
        expectQuery(DeclDb::queryType(type), false);
        DeclDb::addForwardDeclaration(enums[0]);
        expectQuery(DeclDb::queryType(type), uint64_t{0});
        DeclDb::addDeclIdentity(enums[0], identity(1, 2));
        expectQuery(DeclDb::queryType(type), identity(1, 2));
        EXPECT_TRUE(outputFiles().empty());
    }

    class DeclDbForwardFileTest : public DeclDbTest, public ::testing::WithParamInterface<Format> {};

    TEST_P(DeclDbForwardFileTest, EmptyDatabaseWritesAReadableEmptyList) {
        const ScopedSerializationFormat format{GetParam()};
        DeclDb::serializeForwardDeclarations();
        ASSERT_TRUE(std::filesystem::is_regular_file(forwardPath()));
        EXPECT_EQ(outputFiles(), std::vector<std::filesystem::path>{forwardPath()});
        // Even an empty list carries the source version.
        expectForwards(readForwardFile(), forwardList());
    }

    TEST_P(DeclDbForwardFileTest, GroupsAllOccurrencesByFullIdentityAndPreservesTheirOrder) {
        const ScopedSerializationFormat format{GetParam()};
        auto*                           context = parseCode("struct Record {}; enum class Enum {}; void function() {} struct Unreferenced {};");
        ASSERT_NE(context, nullptr);
        const auto records   = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        const auto enums     = declarations<clang::EnumDecl>(context->getTranslationUnitDecl());
        const auto functions = declarations<clang::FunctionDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 2u);
        ASSERT_EQ(enums.size(), 1u);
        ASSERT_EQ(functions.size(), 1u);
        // Include values beyond double's exact integer range, with a shared first
        // hash half, so JSON conversion and grouping must preserve both halves.
        DeclDb::addDeclIdentity(records[0], identity(9007199254740993ULL, 18446744073709551615ULL));
        DeclDb::addDeclIdentity(enums[0], identity(9007199254740993ULL, 23));
        DeclDb::addDeclIdentity(functions[0], identity(31, 37));
        DeclDb::addDeclIdentity(records[1], identity(41, 43));
        DeclDb::addForwardDeclaration(records[0]);
        DeclDb::addForwardDeclaration(functions[0]);
        DeclDb::addForwardDeclaration(records[0]);
        DeclDb::addForwardDeclaration(enums[0]);
        DeclDb::addForwardDeclaration(functions[0]);
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList(R"pb(
            forward_declarations { type_id { a: 9007199254740993 b: 18446744073709551615 } occurrence_indices: [0, 2] }
            forward_declarations { type_id { a: 9007199254740993 b: 23 } occurrence_indices: 3 }
            forward_declarations { type_id { a: 31 b: 37 } occurrence_indices: [1, 4] }
        )pb"));
        EXPECT_EQ(outputFiles(), std::vector<std::filesystem::path>{forwardPath()});
    }

    TEST_P(DeclDbForwardFileTest, UnresolvedDefinitionsAreOmittedWithoutLosingTheirHistory) {
        const ScopedSerializationFormat format{GetParam()};
        auto*                           context = parseCode("struct Known {}; struct Pending {};");
        ASSERT_NE(context, nullptr);
        const auto records = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 2u);
        DeclDb::addForwardDeclaration(records[0]);
        DeclDb::addForwardDeclaration(records[1]);
        DeclDb::addForwardDeclaration(records[1]);
        DeclDb::addDeclIdentity(records[0], identity(1, 2));
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList(R"pb(
            forward_declarations { type_id { a: 1 b: 2 } occurrence_indices: 0 }
        )pb"));
        expectQuery(DeclDb::queryDeclIdentity(records[1]), uint64_t{2});
        DeclDb::addDeclIdentity(records[1], identity(3, 4));
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList(R"pb(
            forward_declarations { type_id { a: 1 b: 2 } occurrence_indices: 0 }
            forward_declarations { type_id { a: 3 b: 4 } occurrence_indices: [1, 2] }
        )pb"));
    }

    TEST_P(DeclDbForwardFileTest, RepeatedWritesDoNotAppendOrConsumeOccurrencesAndResetTruncatesTheFile) {
        const ScopedSerializationFormat format{GetParam()};
        auto*                           context = parseCode("struct Record {};");
        ASSERT_NE(context, nullptr);
        const auto records = declarations<clang::RecordDecl>(context->getTranslationUnitDecl());
        ASSERT_EQ(records.size(), 1u);
        DeclDb::addDeclIdentity(records[0], identity(5, 6));
        DeclDb::addForwardDeclaration(records[0]);
        const auto expected = forwardList(R"pb(
            forward_declarations { type_id { a: 5 b: 6 } occurrence_indices: 0 }
        )pb");
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), expected);
        const auto size = std::filesystem::file_size(forwardPath());
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), expected);
        EXPECT_EQ(std::filesystem::file_size(forwardPath()), size);
        EXPECT_EQ(UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence(), 1u);
        DeclDb::reset();
        DeclDb::serializeForwardDeclarations();
        expectForwards(readForwardFile(), forwardList());
        EXPECT_LT(std::filesystem::file_size(forwardPath()), size);
    }

    TEST_P(DeclDbForwardFileTest, FileOpenFailureThrowsAndCanBeRetried) {
        const ScopedSerializationFormat format{GetParam()};
        ASSERT_TRUE(std::filesystem::create_directory(forwardPath()));
        EXPECT_THROW(DeclDb::serializeForwardDeclarations(), std::runtime_error);
        ASSERT_TRUE(std::filesystem::remove(forwardPath()));
        EXPECT_NO_THROW(DeclDb::serializeForwardDeclarations());
        expectForwards(readForwardFile(), forwardList());
    }

    INSTANTIATE_TEST_SUITE_P(Formats, DeclDbForwardFileTest, ::testing::Values(Format::Binary, Format::Json),
                             [](const ::testing::TestParamInfo<Format>& info) { return info.param == Format::Json ? "Json" : "Binary"; });
} // namespace
