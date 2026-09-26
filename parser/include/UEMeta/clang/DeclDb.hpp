#pragma once
#include <variant>
#include <optional>

#include "UEMeta/utility/DeclUtility.hpp"
#include "absl/container/flat_hash_map.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace UEMeta {
    class DeclDb final {
    public:
        using QueryResult = std::variant<Hash, uint64_t, bool, llvm::StringRef, std::monostate>;

        // DeclWrappers call this to bind a themselves to a Hash globally
        static void addDeclIdentity(const clang::Decl* decl, const Hash& hash);

        // returns Hash if the decl is mapped to a full declaration with that identity Hash,
        // string if it's from a system or std:: header,
        // uint64_t if the decl otherwise has a forward declaration, then this is the occurrence index of the latest forward declaration,
        // or false if it's not mapped to anything
        // monostate is returned on exception
        // Function redeclarations resolve to their definition when available.
        // A registered definition's identity takes precedence over its forward-declaration history.
        static QueryResult queryDeclIdentity(const clang::Decl* decl);

        // Returns the Decl* associated with the Hash, or nullptr if no such Decl exists.
        static const clang::Decl* queryDecl(const Hash& hash);

        // Query the declaration referenced by a type, resolving aliases and peeling pointers,
        // references and arrays. Pass the original QualType; callers retain it for type spelling.
        // If requested, unwrapped_type receives the canonical type after peeling those layers,
        // before mapping instantiations to source declarations; it is null for a null input.
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
        static QueryResult queryType(clang::QualType type, clang::QualType* unwrapped_type = nullptr);

        static void serializeIfNeeded(clang::EnumDecl* decl);
        static void serializeIfNeeded(clang::VarDecl* decl);
        static void serializeIfNeeded(clang::RecordDecl* decl);
        static void serializeIfNeeded(clang::FunctionDecl* decl);
        static void serializeIfNeeded(clang::NamespaceDecl* decl); // for unreal extensions; effectively disabled in repl

        // Adds a forward occurrence keyed by the given record, enum or function definition.
        // Throws if for_decl is null, another declaration kind, or not a definition.
        // Does not add for_decl to the visited decls list, nor does it require that for_decl has been encountered/serialized already.
        static void addForwardDeclaration(clang::Decl* for_decl);

        // Marks a top-level output candidate as visited, preventing duplicate serialization.
        // Wrappers use this for candidates they consume, such as nested records and enums;
        // member-only nodes do not need marking just because a wrapper encounters them.
        static void addDeclarationAsVisited(clang::Decl* decl);

        // Waits for all queued and running serialization tasks; call from the thread reading the AST.
        // Detached task failures are logged by the tasks themselves.
        static void awaitPendingSerializations();

        // Serializes known forward declarations to a ForwardDeclarationList and saves it.
        static void serializeForwardDeclarations();

        static void addMethodIdentity(const clang::CXXMethodDecl* decl, const Hash& hash);
        static std::optional<Hash> getMethodIdentity(const clang::CXXMethodDecl* decl);

        static void reset();
    private:
        DeclDb() = default;

        // maps non-forward, non-alias declarations to their serialized identity
        static llvm::DenseMap<const clang::Decl*, Hash> decl_to_identity_map;

        static absl::flat_hash_map<Hash, const clang::Decl*> identity_to_decl_map;
        static llvm::DenseMap<const clang::CXXMethodDecl*, Hash> method_identity_map;

        // Maps definition declarations to all forward-declaration occurrence indices in visitation order.
        // Retain the full history for later consumers; identity lookup uses only the latest occurrence.
        static llvm::DenseMap<const clang::Decl*, llvm::SmallVector<uint64_t>> decl_to_forward_decl_occurrence_map;

        // Declarations already considered by the top-level entry points or consumed by wrappers.
        // Visitation does not imply an identity: an embedded anonymous record has no standalone
        // hash, but must still be skipped when the outer visitor reaches its RecordDecl.
        // Class-member eligibility filters handle static fields/methods without wrapper-side entries.
        static llvm::DenseSet<const clang::Decl*> visited_decls;
    };
} // namespace UEMeta
