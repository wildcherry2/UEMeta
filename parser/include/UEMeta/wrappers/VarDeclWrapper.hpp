#pragma once
#include "DeclWrapper.hpp"

namespace UEMeta {
    class VarDeclWrapper final : public DeclWrapper<clang::VarDecl> {
    public:
        explicit VarDeclWrapper(const clang::VarDecl* decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper(decl, arena) {}
        [[nodiscard]] ParserTypes::TLGlobalVariableDeclaration* serialize() const;
    protected:

        [[nodiscard]] std::string computeFQN() const;
        // Decl ID calculation: if templated, type and template details are hashed in with FQN, otherwise it's just the FQN hash
        [[nodiscard]] Hash computeDeclIdWithTemplateDetailsAndType(std::string_view fqn, ParserTypes::TLGlobalVariableDeclaration *p_msg) const;
        [[nodiscard]] bool computeHasIdentity() const;
    };
}