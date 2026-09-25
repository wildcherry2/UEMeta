#pragma once

#include <algorithm>
#include <iterator>
#include <optional>
#include "ProtoAssertions.hpp"
#include "WrapperTest.hpp"
#include "clang/AST/DeclFriend.h"

namespace UEMeta::Testing {
    inline ParserTypes::Parameter parameter(std::string_view name, std::string_view type,
                                            std::optional<std::string_view> default_value = std::nullopt) {
        ParserTypes::Parameter message;
        *message.mutable_name()     = versioned<ParserTypes::VersionedString>(name);
        *message.mutable_type_ref() = builtin(type);
        if (default_value)
            *message.mutable_default_value() = versioned<ParserTypes::VersionedString>(*default_value);
        return message;
    }

    inline ParserTypes::FunctionCommon common(ParserTypes::FunctionKind           kind        = ParserTypes::FUNCTION_KIND_FREE,
                                              std::optional<std::string_view>     return_type = "void",
                                              ParserTypes::FunctionStorageClass   storage     = ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED,
                                              ParserTypes::ConstantEvaluationKind evaluation  = ParserTypes::CONSTANT_EVALUATION_NONE) {
        ParserTypes::FunctionCommon message;
        message.set_kind(kind);
        if (return_type) {
            *message.mutable_return_type() = versionedRef(builtin(*return_type));
        }
        *message.mutable_storage_class()  = versioned<ParserTypes::VersionedFunctionStorageClass>(storage);
        *message.mutable_consteval_kind() = versioned<ParserTypes::VersionedConstantEvaluationKind>(evaluation);
        return message;
    }

    inline Hash functionId(std::string_view name, std::string_view signature = "") {
        boost::hash2::xxh3_128 hasher;
        boost::hash2::hash_append(hasher, boost::hash2::little_endian_flavor{}, name);
        hasher.update(signature.data(), signature.size());
        return Hash{hasher};
    }

    class CallableTest : public WrapperTest {
    protected:
        std::vector<clang::FunctionDecl*> parse(std::string_view code, const std::vector<std::string>& arguments = {}) {
            std::vector<clang::FunctionDecl*> declarations;
            if (auto* context = parseCode(code, "wrapper_fixture.cpp", arguments))
                collect(context->getTranslationUnitDecl(), declarations);
            return declarations;
        }

        static void expectUnregistered(const clang::Decl* declaration) {
            const auto id = DeclDb::queryDeclIdentity(declaration);
            ASSERT_TRUE(std::holds_alternative<bool>(id));
            EXPECT_FALSE(std::get<bool>(id));
        }

    private:
        static void collect(clang::DeclContext* context, std::vector<clang::FunctionDecl*>& declarations) {
            for (auto* declaration : context->decls()) {
                if (auto* friendship = llvm::dyn_cast<clang::FriendDecl>(declaration)) {
                    declaration = friendship->getFriendDecl();
                    if (!declaration)
                        continue;
                }
                if (auto* function_template = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration))
                    declaration = function_template->getTemplatedDecl();
                if (auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration)) {
                    if (!function->isImplicit() && std::find(declarations.begin(), declarations.end(), function) == declarations.end())
                        declarations.push_back(function);
                }
                else if (auto* class_template = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                    collect(class_template->getTemplatedDecl(), declarations);
                else if (auto* nested = llvm::dyn_cast<clang::DeclContext>(declaration))
                    collect(nested, declarations);
            }
        }
    };
} // namespace UEMeta::Testing
