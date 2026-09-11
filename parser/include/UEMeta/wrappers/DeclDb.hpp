#pragma once
#include <variant>
#include "Types.hpp"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Decl.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace UEMeta {
    class DeclDb final {
    public:
        using QueryResult = std::variant<Hash, uint64_t, bool, llvm::StringRef, std::monostate>;

        // DeclWrappers call this to bind a themselves to a Hash globally
        static void addDeclIdentity(clang::Decl* decl, const Hash& hash);

        // returns Hash if the decl is mapped to a full declaration with that identity Hash,
        // string if it's from a system or std:: header,
        // uint64_t if the decl otherwise has a forward declaration, then this is the occurrence index of the latest forward declaration,
        // or false if it's not mapped to anything
        // monostate is returned on exception
        // note that this means forward declarations after the defining declaration are ignored
        static QueryResult queryDeclIdentity(const clang::Decl* decl);

        static clang::Decl* queryDecl(const Hash& hash);

        // returns Hash if the underlying type is mapped to a known full declaration with that identity Hash,
        // string if it's from a system or std:: header,
        // uint64_t if the underlying type is mapped to a forward declaration,
        // true if the type is primitive, or false if we don't know the type at all
        // monostate is returned on exception
        // note that this means forward declarations after the defining declaration are ignored
        static QueryResult queryType(clang::QualType type);

        static void serializeIfNeeded(clang::EnumDecl* decl);
        static void serializeIfNeeded(clang::VarDecl* decl);

        // Adds a new forward declaration for the given declaration.
        // Throws if forDecl is not a definition.
        // Does not add forDecl to the visited decls list, nor does it require that forDecl has been encountered/serialized already.
        static void addForwardDeclaration(clang::TagDecl* forDecl);

        // Marks the declaration as visited, ensuring that it won't be serialized multiple times.
        static void addDeclarationAsVisited(clang::Decl* decl);
    private:
        DeclDb() = default;

        // maps non-forward, non-alias declarations to their serialized identity
        // also doubles as a way to check if we've visited the decl before
        static llvm::DenseMap<clang::Decl*, Hash> decl_to_identity_map;

        static absl::flat_hash_map<Hash, clang::Decl*> identity_to_decl_map;

        // maps non-forward, non-alias declarations to a vector of forward declaration occurrence indices and a Decl
        // note that the vector should only have one Decl*, and it should be the same as the key Decl*.
        // this preserves the order of forward declarations and the actual declarations relative to each other
        static llvm::DenseMap<clang::Decl*, llvm::SmallVector<std::variant<uint64_t, clang::Decl*>>> decl_to_forward_decl_occurrence_map;

        // Set of all visited Decls. Not all Decls get a Hash/identity, but are eligible for visitation
        // anyways from the AST visitor, so we can use this to quickly skip over things we don't care about before
        // using lengthier predicates. For instance, a static field in a class is a VarDecl, but we handle those
        // during RecordDecl parsing, so we can check this set on each VarDecl visit to quickly skip over the
        // double visit.
        static llvm::DenseSet<clang::Decl*> visited_decls;
    };
}