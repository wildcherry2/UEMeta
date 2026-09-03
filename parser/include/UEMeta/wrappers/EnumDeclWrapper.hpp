#pragma once
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"

namespace UEMeta {
    class EnumDeclWrapper : public TypedDeclWrapper<clang::EnumDecl> {
    public:
        explicit EnumDeclWrapper(const clang::EnumDecl *decl) : TypedDeclWrapper(decl) {}
        ~EnumDeclWrapper() noexcept override = default;

        [[nodiscard]] bool serialize(google::protobuf::Message* p_msg_base) const override;
        void onVisit(clang::ASTContext &context) override;

        // this is for the content hash
        template<class Provider, class Hash, class Flavor>
        friend void tag_invoke(boost::hash2::hash_append_tag const& tag, Provider const& provider,
            Hash& h, Flavor const& f, EnumDeclWrapper const* v) {

            tag_invoke(tag, provider, h, f, reinterpret_cast<TypedDeclWrapper const*>(v));
            boost::hash2::hash_append(h, f, v->underlying_type);
            boost::hash2::hash_append(h, f, v->enum_scope);
            // technically, it's possible for enumerators to change order but manually assign values such that the
            // enum as a whole is structurally the same, but if nothing really changed, it won't make a difference
            // later in the pipeline
            for (auto enumerator : v->enumerators) {
                boost::hash2::hash_append(h, f, enumerator.name);
                boost::hash2::hash_append(h, f, enumerator.documentation);
                boost::hash2::hash_append(h, f, enumerator.value);
            }
        }

    protected:
        [[nodiscard]] Hash computeTypeId(std::string_view fqn, clang::ASTContext &ctx) const override;
        bool hasIdentity() override;
        clang::QualType getQualType(clang::ASTContext &ctx) override;

        class EnumConstantWrapper {
        public:
            explicit EnumConstantWrapper(const clang::EnumConstantDecl* decl, clang::ASTContext& ctx);
            std::variant<llvm::StringRef, std::string> name;
            std::string value;
            llvm::StringRef documentation;
        };

    private:
        std::string underlying_type;
        std::vector<EnumConstantWrapper> enumerators;
        ParserTypes::EnumScope enum_scope = ParserTypes::ENUM_SCOPE_UNSCOPED;
    };
}