#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/raw_ostream.h>
#include <quill/bundled/fmt/format.h>

namespace UEMeta {
    template <typename T>
    concept AnyDecl = std::derived_from<T, clang::Decl>;

    template <AnyDecl T = clang::Decl>
    inline constexpr std::string_view WRAPABLE_LABEL = "decl";

    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::EnumDecl> = "enum";
    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::RecordDecl> = "record";
    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::CXXMethodDecl> = "method";
    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::FunctionDecl> = "function";
    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::FieldDecl> = "field";
    template <>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::VarDecl> = "var";

    // Catch this base to preserve declaration context across all specializations.
    class DeclExceptionBase : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    template <AnyDecl T = clang::Decl, AnyDecl RelatedDecl = clang::Decl>
    class DeclException : public DeclExceptionBase {
    public:
        template <typename... Args>
        DeclException(const T* decl, fmtquill::format_string<Args...> fmt, Args&&... args) :
            DeclExceptionBase(generateMessage(decl, fmt, std::forward<Args>(args)...)) {}

        template<typename... Args>
        DeclException(const T* decl, const RelatedDecl* related_decl, fmtquill::format_string<Args...> fmt, Args&&... args) :
            DeclExceptionBase(generateMessage(decl, related_decl, fmt, std::forward<Args>(args)...)) {}
    private:
        template <typename... Args>
        static std::string generateMessage(const T* decl, fmtquill::format_string<Args...> fmt, Args&&... args) {
            std::string              decl_text;
            llvm::raw_string_ostream os{decl_text};
            printDecl(decl, os);
            return fmtquill::format("{}: {}", decl_text, fmtquill::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        static std::string generateMessage(const T* decl, const RelatedDecl* related, fmtquill::format_string<Args...> fmt, Args&&... args) {
            std::string              decl_text;
            llvm::raw_string_ostream os{decl_text};
            printDecl(decl, os);
            os << "\n(related to)\n";
            printDecl(related, os);

            return fmtquill::format("{}: {}", decl_text, fmtquill::format(fmt, std::forward<Args>(args)...));
        }

        template<AnyDecl PrintDeclType>
        static void printDecl(const PrintDeclType* decl, llvm::raw_string_ostream& os) {
            os << WRAPABLE_LABEL<T> << " ";
            if (decl) {
                decl->print(os);
            }
            else {
                os << "<null>";
            }
        }
    };
} // namespace UEMeta
