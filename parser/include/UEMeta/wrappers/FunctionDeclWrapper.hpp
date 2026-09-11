#pragma once

#include "DeclWrapper.hpp"

namespace UEMeta {
    class FunctionDeclWrapper final : public DeclWrapper<clang::FunctionDecl> {
    public:
        FunctionDeclWrapper(const clang::FunctionDecl* const decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper(decl, arena) {}

        [[nodiscard]] ParserTypes::TLFreeFunctionDeclaration* serialize() const;
    protected:
        void putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const;

    private:
        [[nodiscard]] std::string computeFQN() const;
        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
    };
}
