#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper final : public DeclWrapper<clang::EnumDecl> {
    public:
        explicit EnumDeclWrapper(const clang::EnumDecl *decl) : DeclWrapper(decl) {}
        ~EnumDeclWrapper() noexcept override = default;

        using DeclWrapper::serialize;
    protected:
        // returns nullptr if it's an anonymous enum, since that means each enumerator was serialized as a static variable
        ProtoType* serialize(const std::filesystem::path &out_dir, ProtoType *out_msg) const override;

        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
        [[nodiscard]] bool computeHasIdentity() const;
        [[nodiscard]] std::string computeFQN() const;
    };
}