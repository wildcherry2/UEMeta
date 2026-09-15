#pragma once
#include <variant>
#include <vector>
#include "TopLevel.pb.h"
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper final : public DeclWrapper<clang::EnumDecl> {
    public:
        using IntermediateRepresentation = std::variant<std::vector<ParserTypes::TLGlobalVariableDeclaration*>, ParserTypes::TLEnumDeclaration*>;
        explicit EnumDeclWrapper(const clang::EnumDecl* decl, const std::shared_ptr<google::protobuf::Arena>& arena) : DeclWrapper(decl, arena) {}

        [[nodiscard]] IntermediateRepresentation toIntermediateRepresentation() const;
        void                                     toFile() const;

        static void toFile(IntermediateRepresentation&& ir, const std::shared_ptr<google::protobuf::Arena>& arena);

        // Append anonymous enumerators as static constexpr fields; access is resolved by the owning record.
        void serializeAsFields(ParserTypes::AccessSpecifier access, ParserTypes::TLRecordDeclaration* dest) const;

    protected:
        [[nodiscard]] Hash        computeDeclId(std::string_view fqn) const;
        [[nodiscard]] bool        computeHasIdentity() const;
        [[nodiscard]] std::string computeFQN() const;
    };
} // namespace UEMeta
