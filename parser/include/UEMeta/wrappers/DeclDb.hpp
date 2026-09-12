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

        // Query the declaration referenced by a type, resolving aliases and peeling pointers,
        // references and arrays. Pass the original QualType; callers retain it for type spelling.
        // References target source declarations, never generated instantiations. Before lookup,
        // an instantiated record is mapped to its selected primary or partial-specialization
        // declaration; a written explicit specialization retains its own declaration identity.
        // This includes explicitly requested instantiations, which are not explicit specializations.
        // Member enums instantiated from class templates likewise use their source enum declaration.
        // A registered instantiation hash is ignored, even when its source declaration is unknown.
        // If Clang has not selected a pattern for a generated specialization use, return false.
        // These are read-only lookups: no definition is instantiated or serialized here.
        //
        // Returns Hash for a known serialized declaration, string for a system/std header,
        // uint64_t for the latest forward occurrence, or monostate on exception.
        // If no declaration result is known, returns true for builtin/dependent/template-parameter
        // types, otherwise false. Dependent records with known identities return their identities.
        // Forward declarations after a registered definition do not replace its hash.
        static QueryResult queryType(clang::QualType type);

        static void serializeIfNeeded(clang::EnumDecl* decl);
        static void serializeIfNeeded(clang::VarDecl* decl);
        static void serializeIfNeeded(clang::RecordDecl* decl);

        // Adds a new forward declaration for the given declaration.
        // Throws if forDecl is not a definition.
        // Does not add forDecl to the visited decls list, nor does it require that forDecl has been encountered/serialized already.
        static void addForwardDeclaration(clang::TagDecl* forDecl);

        // Marks a top-level output candidate as visited, preventing duplicate serialization.
        // Wrappers use this for candidates they consume, such as nested records and enums;
        // member-only nodes do not need marking just because a wrapper encounters them.
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

        // Declarations already considered by the top-level entry points or consumed by wrappers.
        // Visitation does not imply an identity: an embedded anonymous record has no standalone
        // hash, but must still be skipped when the outer visitor reaches its RecordDecl.
        // Class-member eligibility filters handle static fields/methods without wrapper-side entries.
        static llvm::DenseSet<clang::Decl*> visited_decls;
    };
}
