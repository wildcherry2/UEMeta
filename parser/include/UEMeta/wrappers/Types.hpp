#pragma once
#include <concepts>
#include <variant>

#include "TopLevel.pb.h"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/Decl.h"
#include "clang/Basic/Specifiers.h"
#include "llvm/ADT/StringRef.h"
#include "absl/hash/hash.h"

namespace UEMeta {
    template<typename T>
    concept WrapableDecl = std::derived_from<T, clang::NamedDecl>;

    template<typename T>
    concept TagDeclDerived = std::derived_from<T, clang::TagDecl>;

    template<typename T>
    concept TemplateSpecializableDeclType = WrapableDecl<T> && requires(T* a)
    {
        {a->getTemplateSpecializationKind()} -> std::same_as<clang::TemplateSpecializationKind>;
    };

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
}
