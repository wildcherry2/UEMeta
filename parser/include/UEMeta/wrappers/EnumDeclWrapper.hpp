#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"
#include <variant>
#include <vector>

namespace UEMeta {
    class EnumDeclWrapper final : public DeclWrapper<clang::EnumDecl> {
    public:
        using SerializeResult = std::variant<ParserTypes::VariableGroup*, ParserTypes::TLEnumDeclaration*>;
        explicit EnumDeclWrapper(const clang::EnumDecl *decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper(decl, arena) {}

        [[nodiscard]] SerializeResult serialize() const;
        // todo make overloads to serialize to fields/vars
    protected:
        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
        [[nodiscard]] bool computeHasIdentity() const;
        [[nodiscard]] std::string computeFQN() const;
    };
}