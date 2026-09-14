#pragma once
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "TopLevel.pb.h"
#include "UEMeta/Cli.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/Decl.h"
#include "clang/Basic/Specifiers.h"
#include "llvm/ADT/StringRef.h"
#include "absl/hash/hash.h"
#include "clang/AST/DeclCXX.h"

namespace UEMeta {
    template<typename T>
    concept WrapableDecl = std::same_as<clang::VarDecl, T>
        || std::same_as<clang::FieldDecl, T>
        || std::same_as<clang::FunctionDecl, T>
        || std::same_as<clang::EnumDecl, T>
        || std::same_as<clang::RecordDecl, T>
        || std::same_as<clang::CXXMethodDecl, T>;

    template<typename T>
    concept TagDeclDerived = std::derived_from<T, clang::TagDecl>;

    template<typename T>
    concept TemplateSpecializableDeclType = WrapableDecl<T> && requires(T* a)
    {
        {a->getTemplateSpecializationKind()} -> std::same_as<clang::TemplateSpecializationKind>;
    };

    template<typename T>
    concept ProtoMessage = std::derived_from<T, google::protobuf::Message>;

    using AnyString = std::variant<std::string, std::string_view, llvm::StringRef>;

    struct Hash {
        union {
            uint64_t raw[2];
            struct {
                uint64_t a;
                uint64_t b;
            };
        };

        explicit Hash(boost::hash2::xxh3_128& hasher);
        Hash() = default;

        void putProtoHash(ParserTypes::Hash* hash) const;

        friend bool operator==(const Hash &Lhs, const Hash &Rhs) {
            return Lhs.a == Rhs.a
                   && Lhs.b == Rhs.b;
        }

        friend bool operator!=(const Hash &Lhs, const Hash &Rhs) {
            return !(Lhs == Rhs);
        }

        template <typename H>
        friend H AbslHashValue(H state, const Hash& h) {
            return H::combine(std::move(state), h.a, h.b);
        }

        explicit operator bool() const {
            return a || b;
        }
    };

    template<typename T>
    concept Stringish = std::same_as<T, llvm::StringRef> || std::same_as<T, std::string> || std::same_as<T, std::string_view>;

    template<Stringish ValueType>
    void SetVersionedString(ParserTypes::VersionedString* p_msg, const ValueType& value) {
        const std::string& version_str = Config::GetConfig().Version();
        ParserTypes::VersionedString_VersionItem* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        if constexpr(std::same_as<ValueType, std::string>) {
            p_version->set_value(value);
        }
        else if constexpr (std::same_as<ValueType, std::string_view>) {
            p_version->set_value(std::string(value));
        }
        else {
            p_version->set_value(value.str());
        }
    }

    template<typename MessageType, typename ValueType>
    void SetVersioned(MessageType* p_msg, ValueType value) {
        const std::string& version_str = Config::GetConfig().Version();
        auto* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        p_version->set_value(value);
    }

    inline void SetVersionedBool(ParserTypes::VersionedBool* p_msg, const bool value) {
        const std::string& version_str = Config::GetConfig().Version();
        if (value) {
            p_msg->add_true_versions(version_str);
        }
        else {
            p_msg->add_false_versions(version_str);
        }
    }

    inline void SetVersionedUint64List(ParserTypes::VersionedUint64List* p_msg, const std::vector<uint64_t>& value_vec) {
        const std::string& version_str = Config::GetConfig().Version();
        auto* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        for (const uint64_t value : value_vec) {
            p_version->add_value(value);
        }
    }

    static uint64_t allocateDeclOccurrence();
}
