#include "UEMeta/clang/ReflectionDb.hpp"

#include <codecvt>
#include <concepts>
#include <locale>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "UEMeta/Cli.hpp"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "UEMeta/utility/DeclException.hpp"

#define REFL_PRED if (!unrealEnabled()) return {}; \
    if (!decl) return {}; \
    if (const auto existing_it = decl_to_package_name_map.find(decl); existing_it != decl_to_package_name_map.end()) { \
        return existing_it->second; \
    }

llvm::DenseMap<clang::FileID, std::set<UEMeta::ReflectionDb::ReflectionMacro, std::less<>>> UEMeta::ReflectionDb::file_to_reflection_macro_map;
llvm::DenseMap<clang::FileID, std::set<UEMeta::ReflectionDb::DeclWithSource, std::less<>>>  UEMeta::ReflectionDb::file_to_decl_source_map;
llvm::DenseMap<clang::FileID, std::string_view>                                             UEMeta::ReflectionDb::file_to_reflected_package_map;
std::unordered_map<std::filesystem::path, std::string>                                      UEMeta::ReflectionDb::package_root_to_package_name_map;
llvm::DenseMap<const clang::Decl*, std::string_view>                                        UEMeta::ReflectionDb::decl_to_package_name_map;

void UEMeta::ReflectionDb::addReflectionMacro(clang::FileID file_id, ParserTypes::ReflectionKind kind, unsigned begin_offset, unsigned end_offset,
                                              clang::SourceManager& source_manager) {
    if (!unrealEnabled()) return;
    if (const auto set_it = file_to_reflection_macro_map.find(file_id); set_it != file_to_reflection_macro_map.end()) {
        set_it->getSecond().emplace_hint(set_it->getSecond().end(), file_id, kind, begin_offset, end_offset);
    }
    else if (!file_to_reflection_macro_map
              .try_emplace(file_id,
                           std::set<ReflectionMacro, std::less<>>({
                               ReflectionMacro{.file_id = file_id, .kind = kind, .begin_offset = begin_offset, .end_offset = end_offset}
                           }))
              .second) {
        throw std::runtime_error("Somehow failed to emplace a non-existent reflection macro set!");
    }

    computePackageIfNeeded(file_id, source_manager);
}

std::string_view UEMeta::ReflectionDb::registerReflectable(const clang::RecordDecl* decl) {
    REFL_PRED;
    if (!decl->isThisDeclarationADefinition()) return {};
    static constexpr FlagT REFL_FLAGS = ParserTypes::REFLECTION_KIND_UCLASS | ParserTypes::REFLECTION_KIND_USTRUCT |
                                        ParserTypes::REFLECTION_KIND_UINTERFACE;
    const std::string_view package = getPackageIfReflected(decl, decl->getBeginLoc(), decl->getBraceRange().getBegin(), REFL_FLAGS);

    // we intentionally map a potentially empty string view so that duplicate calls don't go through tryGetPackage
    return decl_to_package_name_map.emplace_or_assign(decl, package).first->second;
}

std::string_view UEMeta::ReflectionDb::registerReflectable(const clang::CXXMethodDecl* decl) {
    REFL_PRED;
    clang::SourceRange range = decl->getSourceRange();
    if (decl->hasInlineBody() || decl->doesThisDeclarationHaveABody()) {
        if (const auto* body = decl->getBody()) {
            range.setEnd(body->getBeginLoc());
        }
    }
    const std::string_view package = getPackageIfReflected(decl, range.getBegin(), range.getEnd(), ParserTypes::REFLECTION_KIND_UFUNCTION);

    // we intentionally map a potentially empty string view so that duplicate calls don't go through tryGetPackage
    return decl_to_package_name_map.emplace_or_assign(decl, package).first->second;
}

std::string_view UEMeta::ReflectionDb::registerReflectable(const clang::EnumDecl* decl) {
    REFL_PRED;
    if (!decl->isThisDeclarationADefinition()) return {};
    const std::string_view package = getPackageIfReflected(decl, decl->getBeginLoc(), decl->getBraceRange().getBegin(), ParserTypes::REFLECTION_KIND_UENUM);

    // we intentionally map a potentially empty string view so that duplicate calls don't go through tryGetPackage
    return decl_to_package_name_map.emplace_or_assign(decl, package).first->second;
}

std::string_view UEMeta::ReflectionDb::registerReflectable(const clang::FieldDecl* decl) {
    REFL_PRED;
    const clang::SourceRange range   = decl->getSourceRange();
    const std::string_view   package = getPackageIfReflected(decl, range.getBegin(), range.getEnd(), ParserTypes::REFLECTION_KIND_UPROPERTY);
    return decl_to_package_name_map.emplace_or_assign(decl, package).first->second;
}

std::string_view UEMeta::ReflectionDb::registerReflectable(const clang::NamespaceDecl* decl) {
    REFL_PRED;
    // for this to match the case where a namespace is reflected as a UEnum, the namespace must have exactly one unscoped enum declaration within it
    if (std::distance(decl->decls_begin(), decl->decls_end()) != 1) return {};
    const auto* enum_decl = llvm::dyn_cast_or_null<clang::EnumDecl>(*decl->decls_begin());
    if (!enum_decl || !enum_decl->isThisDeclarationADefinition() || enum_decl->isScoped()) return {};

    const clang::SourceLocation begin = decl->getBeginLoc();
    const clang::SourceLocation end = begin.getLocWithOffset(10 + static_cast<int>(decl->getName().size()));
    const std::string_view package = getPackageIfReflected(decl, begin, end, ParserTypes::REFLECTION_KIND_UENUM);
    decl_to_package_name_map.emplace_or_assign(decl, std::string_view{package});
    decl_to_package_name_map.emplace_or_assign(enum_decl, std::string_view{package});

    return package;
}

void UEMeta::ReflectionDb::computePackageIfNeeded(clang::FileID file_id, clang::SourceManager& source_manager) {
    if (file_to_reflected_package_map.contains(file_id))
        return;
    // fastest cache missed, disassemble path and see if we already know the package

    const std::filesystem::path real_path = [file_id, &source_manager] {
        const auto file_entry = source_manager.getFileEntryRefForID(file_id);
        if (!file_entry)
            throw std::runtime_error("Given file ID that doesn't map to a file!");
        const auto path = file_entry->getFileEntry().tryGetRealPathName();
        if (path.empty())
            throw std::runtime_error("Given file ID that maps to a virtual file!");
        return std::filesystem::path{path.begin(), path.end()};
    }();

    const auto pathStringToPackage = []<typename T = std::filesystem::path::string_type>(const T& path) -> std::optional<std::string> {
        if constexpr (std::same_as<T, std::wstring>) {
            constexpr std::wstring_view suffix = L".Build.cs";
            if (path.size() <= suffix.size() || !path.ends_with(suffix))
                return std::nullopt;
            static std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>> converter;
            return converter.to_bytes(path.data(), path.data() + path.size() - suffix.size());
        }
        else {
            constexpr std::string_view suffix = ".Build.cs";
            if (path.size() <= suffix.size() || !path.ends_with(suffix))
                return std::nullopt;
            return path.substr(0, path.size() - suffix.size());
        }
    };

    const auto pathToFileId = [&source_manager](const std::filesystem::path& path) -> std::optional<clang::FileID> {
        auto file = source_manager.getFileManager().getFileRef(path.string(), false, false);
        if (!file)
            return std::nullopt;
        const clang::FileID known_file_id = source_manager.translateFile(*file);
        if (known_file_id.isInvalid())
            return std::nullopt;
        return known_file_id;
    };

    const auto pathLikelyOutOfScope = []<typename T = std::filesystem::path::string_type>(const T& path) {
        static const T& delimiter              = Config::getConfig().getFileDelimiter();
        static const T  delimiter_backslash    = delimiter + static_cast<std::filesystem::path::value_type>('\\');
        static const T  delimiter_forwardslash = delimiter + static_cast<std::filesystem::path::value_type>('/');
        return path.ends_with(delimiter) || path.ends_with(delimiter_backslash) || path.ends_with(delimiter_forwardslash);
    };

    // search for existing package along parent directories, mapping out any files known to the source manager as we do
    std::filesystem::path              current_path = real_path.parent_path();
    std::vector<clang::FileID>         pending_file_ids;
    std::vector<std::filesystem::path> pending_roots;
    while (!current_path.empty() && !pathLikelyOutOfScope(current_path.native())) {
        // check if the current_path is mapped to a package
        if (const auto package_it = package_root_to_package_name_map.find(current_path); package_it != package_root_to_package_name_map.end()) {
            file_to_reflected_package_map.emplace_or_assign(file_id, package_it->second);
            return;
        }

        // current_path is not mapped to a package, look at all of the files under the current_path directory
        // to see if any of them are the Build.cs file we're looking for; even when we do find the package,
        // we finish iterating through the directory so that all files in this directory that are being used
        // already have their package mapped, reducing I/O
        std::string_view package;
        for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
            if (!entry.is_regular_file())
                continue;
            // since we haven't found the package yet, check to see if this file is what we're looking for
            if (package.empty()) {
                // if it is what we're looking for, map this directory and any child directories we've gone through,
                // as well as any discovered FileIDs and the passed in FileID, to the package
                if (auto package_name = pathStringToPackage(entry.path().filename().native())) {
                    package = package_root_to_package_name_map.emplace(current_path, std::move(*package_name)).first->second;
                    for (const auto& pending_root : pending_roots) {
                        package_root_to_package_name_map.emplace(pending_root, std::string{package});
                    }

                    file_to_reflected_package_map.emplace_or_assign(file_id, package);
                    for (const auto pending_file_id : pending_file_ids) {
                        file_to_reflected_package_map.emplace_or_assign(pending_file_id, package);
                    }

                    pending_roots.clear();
                    pending_file_ids.clear();
                    continue; // prevents redundant pathToFileId call since .cs files won't have a FileID
                }
            }

            // regardless of whether we know the package, if this file (which is implicitly not the file we're looking for)
            // has a FileID, we can map it if we know the package or add it to the pending IDs if we don't.
            if (const auto known_file_id = pathToFileId(entry.path())) {
                if (package.empty())
                    pending_file_ids.push_back(*known_file_id);
                else
                    file_to_reflected_package_map.emplace_or_assign(*known_file_id, package);
            }
        }

        if (!package.empty())
            return;
        const auto parent_path = current_path.parent_path();
        if (parent_path == current_path)
            break;
        pending_roots.push_back(current_path);
        current_path = parent_path;
    }

#if defined(NDEBUG) && !defined(UEM_TESTING)
    UEM_WARN("Failed to find package for file at path {}, this may not be an Unreal compilation!", real_path.string());
#endif
}

bool UEMeta::ReflectionDb::unrealEnabled() {
    return Config::getConfig().unrealExtensionsEnabled();
}

std::string_view UEMeta::ReflectionDb::getPackageIfReflected(const clang::Decl* decl, clang::SourceLocation begin, clang::SourceLocation end,
                                                             FlagT assert_refl_kind) {
    if (begin.isInvalid() || end.isInvalid()) return {};
    auto [begin_file, begin_offset] = decl->getASTContext().getSourceManager().getDecomposedExpansionLoc(begin);
    auto [end_file, end_offset]     = decl->getASTContext().getSourceManager().getDecomposedExpansionLoc(end);

    if (begin_file != end_file) return {};
    if (begin_offset == end_offset) return {};

    // add this decl's source location and get the previous decl from the result
    const DeclWithSource* previous_decl = nullptr;
    if (auto src_set_it = file_to_decl_source_map.find(begin_file); src_set_it != file_to_decl_source_map.end()) {
        auto empl_result = src_set_it->second.emplace(decl, begin_offset, end_offset);
        if (empl_result.first != src_set_it->second.begin())
            previous_decl = &*std::prev(empl_result.first);
    }
    else {
        file_to_decl_source_map.emplace_or_assign(begin_file, std::set<DeclWithSource, std::less<>>({
                                                      DeclWithSource{.decl = decl, .begin_offset = begin_offset, .end_offset = end_offset}
                                                  }));
    }

    // Macro end offsets are exclusive: a macro may end exactly where the declaration begins.
    const ReflectionMacro* previous_macro = nullptr;
    if (auto refl_macro_it = file_to_reflection_macro_map.find(begin_file); refl_macro_it != file_to_reflection_macro_map.end()) {
        const auto next_macro = refl_macro_it->second.upper_bound(begin_offset);
        if (next_macro != refl_macro_it->second.begin()) {
            previous_macro = &*std::prev(next_macro);
        }
    }

    // if there aren't any macros before this declaration, then it's definitely not reflected
    if (!previous_macro) return {};

    // A macro preceding another declaration does not annotate this one, regardless of its kind.
    if (previous_decl && previous_macro->end_offset <= previous_decl->end_offset)
        return {};

    if (!(previous_macro->kind & assert_refl_kind)) {
        throw DeclException(decl, "Reflection macro found, but it was recorded as type {}!", static_cast<FlagT>(previous_macro->kind));
    }

    return getPackageOrThrow(decl, begin_file);
}

std::string_view UEMeta::ReflectionDb::getPackageOrThrow(const clang::Decl* decl, clang::FileID file_id) {
    if (const auto pkg_name_it = file_to_reflected_package_map.find(file_id); pkg_name_it != file_to_reflected_package_map.end()) {
        return pkg_name_it->second;
    }

    throw DeclException(decl, "Failed to find unreal package for declaration!");
}

#ifdef UEM_TESTING
void UEMeta::ReflectionDb::reset() {
    file_to_reflection_macro_map.clear();
    file_to_decl_source_map.clear();
    file_to_reflected_package_map.clear();
    package_root_to_package_name_map.clear();
}
#endif

#undef REFL_PRED
