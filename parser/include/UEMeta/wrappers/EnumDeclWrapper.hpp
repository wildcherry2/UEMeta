#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper : public TypedDeclWrapper<clang::EnumDecl> {
    public:
        explicit EnumDeclWrapper(const clang::EnumDecl *decl) : TypedDeclWrapper(decl) {}
        ~EnumDeclWrapper() noexcept override = default;
        void serialize(const std::filesystem::path &to_dir) override;
        void onVisit(clang::ASTContext &context) override;
    protected:
        uint64_t computeContentHash(clang::ASTContext &ctx) override;
        uint64_t computeTypeId(std::string_view fqn, clang::ASTContext &ctx) override;
        bool isAnonymous() override;
        clang::QualType getQualType(clang::ASTContext &ctx) override;
    };
}