#pragma once
#include <concepts>
#include <filesystem>
#include <vector>
#include <variant>
#include <string>
#include <string_view>

#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/QualTypeNames.h"

namespace UEMeta {

    template<typename T>
    concept DerivedDeclType = std::derived_from<T,  clang::NamedDecl>;

    class DeclWrapper {
    public:
        virtual ~DeclWrapper() noexcept = default;
        virtual void serialize(const std::filesystem::path& to_dir) = 0;
        void addForwardDeclaration();

    protected:
        std::vector<uint64_t> forward_declarations{};
        static uint64_t allocateDeclOccurrence();
    };

    template<DerivedDeclType DerivedDeclType>
    class TypedDeclWrapper : public DeclWrapper {
    public:
        ~TypedDeclWrapper() noexcept override = default;

        explicit TypedDeclWrapper(const DerivedDeclType* decl) : decl(decl->getCanonicalDecl()) {}

        virtual void onVisit(clang::ASTContext& context) {
            const clang::SourceManager& source_manager = context.getSourceManager();
            file_location = source_manager.getFilename(source_manager.getExpansionLoc(decl->getLocation()));
            if (const clang::RawComment* comment = context.getRawCommentForDeclNoCache(decl)) {
                documentation = comment->getRawText(source_manager);
            }

            // todo may want to get the desugared name
            simple_name = decl->getDeclName().isIdentifier() ? std::variant<llvm::StringRef, std::string>{decl->getName()}
            : std::variant<llvm::StringRef, std::string>(decl->getNameAsString());

            occurrence_index = allocateDeclOccurrence();

            content_hash = computeContentHash(context);
            if (content_hash == 0) {
                throw std::runtime_error("Invalid content hash!");
            }

            is_anonymous = isAnonymous();

            clang::QualType qual_type = getQualType(context);
            if (qual_type.isNull()) {
                throw std::invalid_argument("Failed to construct QualType for TypedDeclWrapper!");
            }
            qual_type = qual_type.getDesugaredType(context);

            fqn = clang::TypeName::getFullyQualifiedName(qual_type, context, context.getPrintingPolicy(), true);
            if (fqn.empty()) {
                throw std::runtime_error("Failed to construct FQN!");
            }

            type_id = computeTypeId(fqn, context);
            if (type_id == 0) {
                throw std::runtime_error("Invalid type_id!");
            }
        }

    protected:
        const DerivedDeclType* decl{};
        llvm::StringRef file_location;
        llvm::StringRef documentation;
        std::variant<llvm::StringRef, std::string> simple_name;
        uint64_t occurrence_index{};
        uint64_t content_hash{};
        uint64_t type_id{};
        std::string fqn{};
        bool is_anonymous{};

        virtual uint64_t computeContentHash(clang::ASTContext& ctx) = 0;
        virtual uint64_t computeTypeId(std::string_view fqn, clang::ASTContext& ctx) = 0;
        virtual bool isAnonymous() = 0;
        virtual clang::QualType getQualType(clang::ASTContext& ctx) = 0;
    };
}
