#pragma once
#include <stdexcept>

#include "clang/Basic/IdentifierTable.h"
#include "clang/Lex/Token.h"
#include <quill/bundled/fmt/format.h>

namespace UEMeta {
    class TokenException : public std::runtime_error {
    public:
        template <typename... Args>
        TokenException(const clang::Token& token, fmtquill::format_string<Args...> fmt, Args&&... args) :
            std::runtime_error(generateMessage(token, fmt, std::forward<Args>(args)...)) {}

    private:
        template <typename... Args>
        static std::string generateMessage(const clang::Token& token, fmtquill::format_string<Args...> fmt, Args&&... args) {
            const auto* id = token.getIdentifierInfo();
            std::string_view name = id ? std::string_view{id->getName().data(), id->getName().size()} : "(unknown token)";
            return fmtquill::format("token {}: {}", name, fmtquill::format(fmt, std::forward<Args>(args)...));
        }
    };
}
