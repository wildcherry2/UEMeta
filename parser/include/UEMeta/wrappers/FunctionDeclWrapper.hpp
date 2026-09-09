#pragma once

#include "DeclWrapper.hpp"

namespace UEMeta {
    class FunctionDeclWrapper final : public DeclWrapper<clang::FunctionDecl> {
    public:
        explicit FunctionDeclWrapper(const clang::FunctionDecl* decl): DeclWrapper(decl) {}
        ~FunctionDeclWrapper() noexcept override = default;

        using DeclWrapper::serialize;
        void serialize(const std::filesystem::path& out_dir, ProtoType *out_msg) const override;
    protected:
        void putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const;

    private:
        [[nodiscard]] std::string computeFQN() const;
        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const;
    };
}
