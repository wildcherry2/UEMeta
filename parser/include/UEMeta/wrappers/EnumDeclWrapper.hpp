#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper final : public DeclWrapper {
    public:
        explicit EnumDeclWrapper(const clang::EnumDecl *decl) : DeclWrapper(decl) {}
        ~EnumDeclWrapper() noexcept override = default;

        [[nodiscard]] bool serialize(const std::filesystem::path &out_dir) const override;
        [[nodiscard]] std::vector<google::protobuf::Message*> serialize() const override;

        void onVisit(clang::ASTContext &context) override;

    protected:
        [[nodiscard]] Hash computeTypeId(std::string_view fqn, clang::ASTContext &ctx) const override;

        [[nodiscard]] bool hasIdentity() const override;
        clang::QualType getQualType(clang::ASTContext &ctx) override;

        void addToContentHash(boost::hash2::xxh3_128 &hash) const override;

        class EnumConstantWrapper {
        public:
            explicit EnumConstantWrapper(const clang::EnumConstantDecl* decl, clang::ASTContext& ctx);
            std::variant<llvm::StringRef, std::string> name;
            std::string value;
            llvm::StringRef documentation;
        };

    private:
        std::string underlying_type;
        std::vector<EnumConstantWrapper> enumerators;
        ParserTypes::EnumScope enum_scope = ParserTypes::ENUM_SCOPE_UNSCOPED;

        [[nodiscard]] const clang::EnumDecl* Decl() const { return getUnderlyingUnsafe<clang::EnumDecl>(); }
    };
}