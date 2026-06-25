#pragma once
#include "UEMeta/Cli.hpp"
#include "parser.pb.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <ranges>
#include <string_view>
#include "UEMeta/ClangHandler.hpp"

using namespace ParseResult;
using Data = UEMeta::ClangHandler::TransientData;

/// @brief Tracks whether any guarded Clang callback caught an exception.
inline std::atomic_bool GClangExceptionCaught{false};

/// @brief Records a Clang processing exception and logs the failing step.
inline void LogClangException(const std::string_view step, const std::exception& ex) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with exception: {}", step, ex.what());
    } catch (...) {
    }
}

/// @brief Records an unknown Clang processing exception and logs the failing step.
inline void LogClangUnknownException(const std::string_view step) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with unknown exception", step);
    } catch (...) {
    }
}

/// @brief Runs a Clang callback with exception logging and failure recording.
template <typename Func>
void GuardClangCallback(const std::string_view step, Func&& func) noexcept {
    try {
        std::forward<Func>(func)();
    } catch (const std::exception& ex) {
        LogClangException(step, ex);
    } catch (...) {
        LogClangUnknownException(step);
    }
}

/// @brief Gets the declaration as a macro-expanded string
inline std::string_view GetDeclAsString(const Data& data, const clang::Decl* decl) {
    static llvm::DenseMap<const clang::Decl*, std::string> decl_contents;
    if (!decl) throw std::runtime_error("Can't get a string from a null decl!");
    if (const auto cached = decl_contents.find(decl); cached != decl_contents.end()) {
        return std::string_view{cached->getSecond()};
    }
    std::string s{};
    llvm::raw_string_ostream os(s);
    decl->print(os, data.context->getPrintingPolicy(), 0, true);
    os.flush();
    decl_contents[decl] = std::move(s);
    return decl_contents[decl];
}

inline llvm::StringRef GetDeclSourcePath(const Data& data, const clang::Decl* decl) {
    static llvm::DenseMap<const clang::Decl*, llvm::StringRef> decl_srcs;
    if (!decl) throw std::runtime_error("Can't get a source location from a null decl!");
    if (const auto cached = decl_srcs.find(decl); cached != decl_srcs.end()) {
        return cached->second;
    }
    const auto& source_man = data.context->getSourceManager();
    const auto source_loc = source_man.getExpansionLoc(decl->getLocation());
    decl_srcs[decl] = source_man.getFilename(source_loc);
    return decl_srcs[decl];
}

inline uint64_t GetDeclSourcePathHash(llvm::StringRef in) {
    static llvm::DenseMap<llvm::StringRef, uint64_t> hashes;
    if (const auto hash = hashes.find(in); hash != hashes.end()) {
        return hash->getSecond();
    }
    hashes[in] = std::hash<std::string>::operator()(in.str());
    return hashes[in];
}

/// @brief Fills out an EnumDetails* from an EnumDecl*
inline void PopulateEnumDetails(const Data& data, EnumDetails* p_msg, const clang::EnumDecl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateEnumDetails due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    auto underlying = decl->getIntegerType();
    if (!underlying.isNull()) {
        p_msg->set_underlying_type(underlying.getAsString(data.context->getPrintingPolicy()));
    }
    p_msg->set_scope(!decl->isScoped() ? ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ENUM_SCOPE_CLASS : ENUM_SCOPE_STRUCT);
}


/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(const Data& data, Identifier* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    if (auto* as_named = llvm::dyn_cast_or_null<clang::NamedDecl>(decl)) {
        p_msg->set_qualified_name(as_named->getQualifiedNameAsString());
        p_msg->set_qualified_name_hash(std::hash<std::string>::operator()(p_msg->qualified_name()));
        p_msg->set_name(as_named->getName());
        p_msg->set_file_path_hash(GetDeclSourcePathHash(GetDeclSourcePath(data, decl)));
        if (const auto* comment = data.context->getRawCommentForDeclNoCache(decl)) {
            p_msg->set_documentation(comment->getRawText(data.context->getSourceManager()).str());
        }
        auto scopes = std::views::split(p_msg->qualified_name(), std::string_view{"::"})
                        | std::views::transform([](auto range) -> std::string_view {
                                auto sv = std::string_view{range};
                                auto start = sv.find_first_not_of(" \t\n\r");
                                if (start == std::string_view::npos) return ""; // All whitespace

                                // Find last non-whitespace character
                                auto end = sv.find_last_not_of(" \t\n\r");
                                return sv.substr(start, end - start + 1);
                            })
                        | std::views::filter([] (auto sv) { return !sv.empty(); });
        for (auto scope : scopes) {
            p_msg->add_scope(std::string{scope}); // proto3 needs copies of string views to serialize properly
        }
    }
}

/// @brief Fills out a DeclarationMetadata from a Decl
inline void PopulateDeclarationMetadata(const Data& data, DeclarationMetadata* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateDeclarationMetadata due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    PopulateIdentifier(data, p_msg->mutable_identifier(), decl);
    p_msg->set_occurrence_index(data.occurrence_index);
    if (auto* as_enum = llvm::dyn_cast_or_null<clang::EnumDecl>(decl)) {
        p_msg->set_is_anonymous(as_enum->getName().empty());
    }
    else if (auto* as_record = llvm::dyn_cast_or_null<clang::RecordDecl>(decl)) {
        p_msg->set_is_anonymous(as_record->isAnonymousStructOrUnion() || as_record->getName().empty()); // catches nested anon structs as well
    }
    p_msg->set_content_hash(std::hash<std::string_view>::operator()(GetDeclAsString(data, decl)));
}

/// @brief Finalizes a top-level forward declaration.
inline void FinalizeTL(Data& data, TLForwardDeclaration* decl) {
    if (!decl) {
        UEM_WARN("Failed to FinalizeTL due to nullptr! TLForwardDeclaration={}", !!decl);
        return;
    }
    auto decl_container = google::protobuf::Arena::Create<Declaration>(&data.arena);
    decl_container->set_allocated_forward_declaration(decl);
    data.results.emplace_back(decl_container);
}

/// @brief Finalizes a top-level enum declaration.
inline void FinalizeTL(Data& data, TLEnumDeclaration* decl) {
    if (!decl) {
        UEM_WARN("Failed to FinalizeTL due to nullptr! TLEnumDeclaration={}", !!decl);
        return;
    }
    auto decl_container = google::protobuf::Arena::Create<Declaration>(&data.arena);
    decl_container->set_allocated_enum_declaration(decl);
    data.results.emplace_back(decl_container);
}
