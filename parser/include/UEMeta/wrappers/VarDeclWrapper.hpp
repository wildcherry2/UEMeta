#pragma once
#include "DeclWrapper.hpp"

namespace UEMeta {
    class VarDeclWrapper final : public DeclWrapper<clang::VarDecl> {
    public:
        explicit VarDeclWrapper(const clang::VarDecl* decl): DeclWrapper(decl) {}
        ~VarDeclWrapper() noexcept override = default;

        using DeclWrapper::serialize;
        void serialize(const std::filesystem::path &out_dir, ProtoType *out_msg) const override;
    protected:

        [[nodiscard]] std::string computeFQN() const;
        // Decl ID calculation: if templated, type and template details are hashed in with FQN, otherwise it's just the FQN hash
        [[nodiscard]] Hash computeDeclIdWithTemplateDetailsAndType(std::string_view fqn, ParserTypes::TLGlobalVariableDeclaration *p_msg) const;
        [[nodiscard]] bool computeHasIdentity() const;
    };
}