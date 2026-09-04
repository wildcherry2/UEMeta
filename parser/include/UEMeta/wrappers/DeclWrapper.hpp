#pragma once
#include <concepts>
#include <filesystem>
#include <vector>
#include <variant>
#include <string>
#include <string_view>

#include "MessageAllocator.hpp"
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
        // ReSharper disable once CppNonExplicitConvertingConstructor
        DeclWrapper(const clang::NamedDecl* decl) : decl(decl) {}
        virtual ~DeclWrapper() noexcept = default;
        [[nodiscard]] virtual bool serialize(const std::filesystem::path& out_dir) const = 0;
        [[nodiscard]] virtual std::vector<google::protobuf::Message*> serialize() const = 0;
        virtual void onVisit(clang::ASTContext& ctx);

        [[nodiscard]] const clang::NamedDecl* getUnderlyingDecl() const;
        void addForwardDeclaration();

        template<DerivedDeclType T>
        [[nodiscard]] const T* getUnderlyingDeclAsOrNull() const {
            return llvm::dyn_cast_or_null<T>(decl);
        }

    protected:
        static uint64_t allocateDeclOccurrence();

        virtual void addToContentHash(boost::hash2::xxh3_128& hash) const;
        void addToMetadata(ParserTypes::DeclarationMetadata* metadata) const;

        [[nodiscard]] virtual Hash computeTypeId(std::string_view fqn, clang::ASTContext &ctx) const = 0;
        [[nodiscard]] virtual bool hasIdentity() const = 0;
        [[nodiscard]] virtual clang::QualType getQualType(clang::ASTContext& ctx) = 0;

        template<DerivedDeclType T>
        [[nodiscard]] const T* getUnderlyingUnsafe() const {
            return llvm::cast<T>(decl);
        }
    private:
        const clang::NamedDecl* decl;
        std::vector<uint64_t> forward_declarations{};
        llvm::StringRef file_location;
        llvm::StringRef documentation;
        uint64_t occurrence_index{};
        Hash type_id{};
        std::string fqn;
        bool has_identity{};
        bool visited{};
    };
}
