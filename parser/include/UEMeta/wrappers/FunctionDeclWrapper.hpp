#pragma once

#include "DeclWrapper.hpp"

namespace UEMeta {
    template<std::derived_from<clang::FunctionDecl> T = clang::FunctionDecl>
    class FunctionDeclWrapper : public DeclWrapper<T> {
    public:
        FunctionDeclWrapper(const T* const decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper<T>(decl, arena) {}

        [[nodiscard]] ParserTypes::TLFreeFunctionDeclaration* serialize() const;
    protected:
        void putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const;

    private:
        [[nodiscard]] std::string computeFQN() const;
        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
    };
}
