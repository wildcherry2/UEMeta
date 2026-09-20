#pragma once
#include "DeclWrapper.hpp"

namespace UEMeta {
    class VarDeclWrapper final : public DeclWrapper<clang::VarDecl> {
    public:
        using IntermediateRepresentation = ParserTypes::TLGlobalVariableDeclaration*;
        explicit VarDeclWrapper(const clang::VarDecl* decl, const std::shared_ptr<google::protobuf::Arena>& arena) : DeclWrapper(decl, arena) {}

        [[nodiscard]] ParserTypes::TLGlobalVariableDeclaration* toIntermediateRepresentation() const;
        void                                                    toFile() const;
        static void toFile(const ParserTypes::TLGlobalVariableDeclaration* ir, const std::shared_ptr<google::protobuf::Arena>& arena);
        static void toString(const ParserTypes::TLGlobalVariableDeclaration* ir, std::string& out);
    private:
        [[nodiscard]] std::string computeFQN() const;
        // Decl ID calculation: if templated, type and template details are hashed in with FQN, otherwise it's just the FQN hash
        [[nodiscard]] Hash computeDeclIdWithTemplateDetailsAndType(std::string_view fqn, ParserTypes::TLGlobalVariableDeclaration* p_msg) const;
    };
} // namespace UEMeta
