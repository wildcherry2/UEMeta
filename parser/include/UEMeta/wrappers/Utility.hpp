#pragma once
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "TopLevel.pb.h"
#include "UEMeta/Cli.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/Specifiers.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "quill/bundled/fmt/format.h"

namespace UEMeta {
    template <typename T>
    concept WrapableDecl = std::same_as<clang::VarDecl, T> || std::same_as<clang::FieldDecl, T> || std::same_as<clang::FunctionDecl, T> ||
                           std::same_as<clang::EnumDecl, T> || std::same_as<clang::RecordDecl, T> || std::same_as<clang::CXXMethodDecl, T>;

    template <typename T>
    concept AnyDecl = std::derived_from<T, clang::Decl>;

    template <typename T>
    concept TopLevelProto = std::same_as<ParserTypes::TLEnumDeclaration, T> || std::same_as<ParserTypes::TLRecordDeclaration, T> ||
                           std::same_as<ParserTypes::TLGlobalVariableDeclaration, T> || std::same_as<ParserTypes::TLFreeFunctionDeclaration, T>;

    template <TopLevelProto T>
    inline constexpr std::string_view TOP_LEVEL_EXT;

    template <>
    inline constexpr std::string_view TOP_LEVEL_EXT<ParserTypes::TLEnumDeclaration> = "enum";
    template <>
    inline constexpr std::string_view TOP_LEVEL_EXT<ParserTypes::TLRecordDeclaration> = "record";
    template <>
    inline constexpr std::string_view TOP_LEVEL_EXT<ParserTypes::TLGlobalVariableDeclaration> = "var";
    template <>
    inline constexpr std::string_view TOP_LEVEL_EXT<ParserTypes::TLFreeFunctionDeclaration> = "function";

    template<AnyDecl T>
    inline constexpr std::string_view WRAPABLE_LABEL = "decl";

    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::EnumDecl> = "enum";
    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::RecordDecl> = "record";
    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::CXXMethodDecl> = "method";
    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::FunctionDecl> = "function";
    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::FieldDecl> = "field";
    template<>
    inline constexpr std::string_view WRAPABLE_LABEL<clang::VarDecl> = "var";

    template <typename T>
    concept TagDeclDerived = std::derived_from<T, clang::TagDecl>;

    template <typename T>
    concept TemplateSpecializableDeclType = (WrapableDecl<T> || std::same_as<clang::CXXRecordDecl, T>) && requires(const T* a) {
        { a->getTemplateSpecializationKind() } -> std::same_as<clang::TemplateSpecializationKind>;
    };

    template <typename T>
    concept ProtoMessage = std::derived_from<T, google::protobuf::Message>;

    using AnyString = std::variant<std::string, std::string_view, llvm::StringRef>;

    struct Hash {
        uint64_t a;
        uint64_t b;

        explicit Hash(boost::hash2::xxh3_128& hasher);
        Hash() = default;

        void putProtoHash(ParserTypes::Hash* hash) const;

        friend bool operator==(const Hash& lhs, const Hash& rhs) { return lhs.a == rhs.a && lhs.b == rhs.b; }

        friend bool operator!=(const Hash& lhs, const Hash& rhs) { return !(lhs == rhs); }

        template <typename H>
        friend H AbslHashValue(H state, const Hash& h) {
            return H::combine(std::move(state), h.a, h.b);
        }

        explicit operator bool() const { return a || b; }
    };

    template <typename T>
    concept Stringish = std::same_as<T, llvm::StringRef> || std::same_as<T, std::string> || std::same_as<T, std::string_view>;

    template <Stringish ValueType>
    void setVersionedString(ParserTypes::VersionedString* p_msg, const ValueType& value) {
        const std::string&                        version_str = Config::getConfig().getVersion();
        ParserTypes::VersionedString_VersionItem* p_version   = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        if constexpr (std::same_as<ValueType, std::string>) {
            p_version->set_value(value);
        }
        else if constexpr (std::same_as<ValueType, std::string_view>) {
            p_version->set_value(std::string(value));
        }
        else {
            p_version->set_value(value.str());
        }
    }

    template <typename MessageType, typename ValueType>
    void setVersioned(MessageType* p_msg, ValueType value) {
        const std::string& version_str = Config::getConfig().getVersion();
        auto*              p_version   = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        p_version->set_value(value);
    }

    inline void setVersionedBool(ParserTypes::VersionedBool* p_msg, const bool value) {
        const std::string& version_str = Config::getConfig().getVersion();
        if (value) {
            p_msg->add_true_versions(version_str);
        }
        else {
            p_msg->add_false_versions(version_str);
        }
    }

    inline void setVersionedUint64List(ParserTypes::VersionedUint64List* p_msg, const std::vector<uint64_t>& value_vec) {
        const std::string& version_str = Config::getConfig().getVersion();
        auto*              p_version   = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        for (const uint64_t value : value_vec) {
            p_version->add_value(value);
        }
    }

    template <AnyDecl T = clang::Decl>
    class DeclException : public std::runtime_error {
    public:
        template <typename... Args>
        DeclException(const T* decl, fmtquill::format_string<Args...> fmt, Args&&... args) :
            std::runtime_error(generateMessage(decl, fmt, std::forward<Args>(args)...)) {}

    private:
        template <typename... Args>
        static std::string generateMessage(const T* decl, fmtquill::format_string<Args...> fmt, Args&&... args) {
            std::string              decl_text;
            llvm::raw_string_ostream os{decl_text};
            if (decl) {
                decl->print(os);
            }
            else {
                os << "<null>";
            }
            return fmtquill::format("{} {}: {}", WRAPABLE_LABEL<T>, decl_text, fmtquill::format(fmt, std::forward<Args>(args)...));
        }
    };
} // namespace UEMeta
