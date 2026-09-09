#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper final : public DeclWrapper<clang::EnumDecl> {
    public:
        explicit EnumDeclWrapper(const clang::EnumDecl *decl) : DeclWrapper(decl) {}
        ~EnumDeclWrapper() noexcept override = default;

        void serialize(const std::filesystem::path &out_dir, ProtoType* out_msg) const override;
        using DeclWrapper::serialize;
    protected:
        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
        [[nodiscard]] bool computeHasIdentity() const;
        [[nodiscard]] std::string computeFQN() const;
    };
}