#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Enums.pb.h"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/DenseMap.h"
#include "UEMeta/utility/DeclUtility.hpp"

namespace UEMeta {
    /**
     * @brief Stores Unreal reflection metadata discovered while preprocessing and parsing.
     */
    class ReflectionDb final {
    public:
        /**
         * @brief Records a reflection macro expansion and discovers its owning package if needed.
         */
        static void addReflectionMacro(clang::FileID         file_id, ParserTypes::ReflectionKind kind, unsigned begin_offset, unsigned end_offset,
                                       clang::SourceManager& source_manager);

        // Marks a declaration as potentially reflected and returns their package if it is reflected.
        // Records, enums, namespaces, methods, fields could be reflected as UClass, UStruct, UEnum, UFunction,
        // UProperty, etc.
        // Adding a Decl to this does not mean that the declaration *is* reflected. It is not a precondition.
        // But if it's reflectable, we'll need it to make reflection queries work.
        // Wrappers should invoke these as they parse.
        // An empty string_view implies the Decl isn't reflected.
        // Calls are idempotent; trying to reregister the same Decl will return the already resolved string_view.
        //  As such, these are also the functions that should be used for queries.
        // Unreal extensions must be enabled from the CLI for this to have any effect.
        static std::string_view registerReflectable(const clang::RecordDecl* decl);
        static std::string_view registerReflectable(const clang::CXXMethodDecl* decl);
        static std::string_view registerReflectable(const clang::EnumDecl* decl);
        static std::string_view registerReflectable(const clang::FieldDecl* decl);
        // in some UE versions, a namespace can be reflected as a UEnum.
        // this can be used to both get the package name and check to see if this is the case
        static std::string_view registerReflectable(const clang::NamespaceDecl* decl);

        static void markEnumAsReflectedNamespace(const clang::EnumDecl* decl);
        static void serializeReflectionCache();
#ifdef UEM_TESTING
        static void reset();
#endif

    private:
        ReflectionDb() = default;
        using FlagT = std::underlying_type_t<ParserTypes::ReflectionKind>;

        static void                            computePackageIfNeeded(clang::FileID file_id, clang::SourceManager& source_manager);
        static bool                            unrealEnabled();
        static std::string_view                getPackageIfReflected(const clang::Decl* decl, clang::SourceLocation begin, clang::SourceLocation end,
                                                                     FlagT assert_refl_kind);

        struct ReflectionMacro final {
            clang::FileID               file_id;
            ParserTypes::ReflectionKind kind{};
            unsigned                    begin_offset{};
            unsigned                    end_offset{};

            friend bool operator<(const ReflectionMacro& lhs, const ReflectionMacro& rhs) { return lhs.end_offset < rhs.end_offset; }
            friend bool operator<(const ReflectionMacro& lhs, const unsigned rhs) { return lhs.end_offset < rhs; }
            friend bool operator<(const unsigned lhs, const ReflectionMacro& rhs) { return lhs < rhs.end_offset; }
        };

        struct DeclWithSource final {
            const clang::Decl* decl{};
            unsigned           begin_offset{};
            unsigned           end_offset{};

            friend bool operator<(const DeclWithSource& lhs, const DeclWithSource& rhs) { return lhs.end_offset < rhs.end_offset; }
        };

        // Maps file IDs to a set of macros ordered by end offset source location for reflection queries
        static llvm::DenseMap<clang::FileID, std::set<ReflectionMacro, std::less<>>> file_to_reflection_macro_map;

        // Maps file IDs to a set of DeclWithSource (caches source location) by end offset source location for reflection queries
        // Since we have to account for nested tags for older unreal versions, end offset calculations for Decls can't be naive;
        // we don't want to get the end offset of the body of a tag, more like the end offset to the tag itself
        static llvm::DenseMap<clang::FileID, std::set<DeclWithSource, std::less<>>> file_to_decl_source_map;

        // Maps file IDs to their unreal package. Any file with a reflection macro should be in this map after preprocessing.
        // This is based on the heuristic that some directory at or above a file will contain a `*.Build.cs`, where the `*` is the
        // package name, and the fact that native code lives under the `/Script/` root package at unreal's runtime.
        // The values are string_views because package_root_to_package_name_map owns the string for less memory usage.
        // This is the first and fastest level of caching for packages. If this misses, we look at the below map.
        static llvm::DenseMap<clang::FileID, std::string_view> file_to_reflected_package_map;

        // Maps a directory path to its package name.
        // To query a file's package when the file_to_reflected_package_map cache misses, you can take apart each part of the
        // query file's path until it matches to a path in this map or we find the Build.cs file; this ensures that you also get the nearest package.
        static std::unordered_map<std::filesystem::path, std::string> package_root_to_package_name_map;

        // Maps decls to their package name
        static llvm::DenseMap<const clang::Decl*, std::string_view> decl_to_package_name_map;

        static llvm::DenseSet<const clang::EnumDecl*> enums_with_refl_ns;
    };
} // namespace UEMeta
