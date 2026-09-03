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
#include "boost/hash2/hash_append_fwd.hpp"
#include "boost/hash2/xxh3.hpp"

namespace UEMeta {

    template<typename T>
    concept DerivedDeclType = std::derived_from<T,  clang::NamedDecl>;

    struct Hash {
        union {
            uint64_t raw[2];
            struct {
                uint64_t a;
                uint64_t b;
            };
        };

        explicit Hash(boost::hash2::xxh3_128& hasher);
        Hash() = default;
    };

    class DeclWrapper {
    public:
        virtual ~DeclWrapper() noexcept = default;

        [[nodiscard]] virtual bool serialize(google::protobuf::Message* p_msg_base) const = 0;

        [[nodiscard]] virtual const clang::NamedDecl* getUnderlyingDecl() const = 0;
        void addForwardDeclaration();

        template<DerivedDeclType T>
        [[nodiscard]] const T* getUnderlyingDeclAsOrNull() const {
            return llvm::dyn_cast_or_null<T>(getUnderlyingDecl());
        }
    protected:
        std::vector<uint64_t>& getForwardDeclarations() { return forward_declarations; }
        static uint64_t allocateDeclOccurrence();

        [[nodiscard]] virtual Hash computeTypeId(std::string_view fqn, clang::ASTContext &ctx) const = 0;
        virtual bool hasIdentity() = 0;
        virtual clang::QualType getQualType(clang::ASTContext& ctx) = 0;

    private:
        std::vector<uint64_t> forward_declarations{};
    };

    template<DerivedDeclType T> //todo most of these could probably be promoted to the untemplated variant, though we'd lose inlining since we'd have to use a virtual to get the decl
    class TypedDeclWrapper : public DeclWrapper {
    public:
        ~TypedDeclWrapper() noexcept override = default;

        explicit TypedDeclWrapper(const T* decl) : decl(decl->getCanonicalDecl()) {}

        // by default, retrieves data needed for DeclarationMetadata
        virtual void onVisit(clang::ASTContext& context) {
            const clang::SourceManager& source_manager = context.getSourceManager();
            file_location = source_manager.getFilename(source_manager.getExpansionLoc(decl->getLocation()));
            if (const clang::RawComment* comment = context.getRawCommentForDeclNoCache(decl)) {
                documentation = comment->getRawText(source_manager);
            }

            occurrence_index = allocateDeclOccurrence();
            has_identity = hasIdentity();

            if (has_identity) {
                const clang::QualType qual_type = getQualType(context);
                if (qual_type.isNull()) {
                    throw std::invalid_argument("Failed to construct QualType for TypedDeclWrapper!");
                }

                fqn = clang::TypeName::getFullyQualifiedName(qual_type, context, context.getPrintingPolicy(), true);
                if (fqn.empty()) {
                    throw std::runtime_error("Failed to construct FQN!");
                }

                type_id = computeTypeId(fqn, context);
                if (type_id.a == 0 && type_id.b == 0) {
                    throw std::runtime_error("Invalid type_id!");
                }
            }
        }

        [[nodiscard]] clang::NamedDecl* getUnderlyingDecl() const override { return decl; }

        // this is for the content hash
        template<class Provider, class Hash, class Flavor>
        friend void tag_invoke(boost::hash2::hash_append_tag const&, Provider const&,
            Hash& h, Flavor const& f, TypedDeclWrapper const* v) {

            boost::hash2::hash_append(h, f, v->file_location);
            boost::hash2::hash_append(h, f, v->documentation);
            boost::hash2::hash_append(h, f, v->occurrence_index);
            boost::hash2::hash_append(h, f, v->type_id.a);
            boost::hash2::hash_append(h, f, v->type_id.b);
            boost::hash2::hash_append(h, f, v->has_identity);
            boost::hash2::hash_append(h, f, v->fqn);
            boost::hash2::hash_append(h, f, v->getForwardDeclarations());
        }

    protected:
        const T* decl{};

    private:
        llvm::StringRef file_location;
        llvm::StringRef documentation;
        uint64_t occurrence_index{};
        Hash type_id{};
        std::string fqn;
        bool has_identity{};
    };
}
