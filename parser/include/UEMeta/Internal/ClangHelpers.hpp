#pragma once
#include "UEMeta/Cli.hpp"
#include "parser.pb.h"
#include <clang/AST/ASTContext.h>

using namespace ParseResult;

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
inline std::string GetDeclAsString(clang::ASTContext* ctx, const clang::Decl* decl) {
    std::string s{};
    llvm::raw_string_ostream os(s);
    decl->print(os, ctx->getPrintingPolicy(), 0, true);
    return s;
}

/// @brief Fills out an EnumDetails* from an EnumDecl*
inline void PopulateEnumDetails(const clang::ASTContext* ctx, EnumDetails* p_msg, const clang::EnumDecl* decl) {
    if (!(ctx && p_msg && decl)) {
        UEM_WARN("Failed to PopulateEnumDetails due to nullptr! ctx={}, p_msg={}, decl={}", !!ctx, !!p_msg, !!decl);
        return;
    }
    auto underlying = decl->getIntegerType();
    if (!underlying.isNull()) {
        p_msg->set_underlying_type(underlying.getAsString(ctx->getPrintingPolicy()));
    }
    p_msg->set_scope(!decl->isScoped() ? ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ENUM_SCOPE_CLASS : ENUM_SCOPE_STRUCT);
}

/// @brief Fills out a DeclarationMetadata from a Decl
inline void PopulateDeclarationMetadata(const clang::ASTContext* ctx, DeclarationMetadata* p_msg, const clang::Decl* decl) {
    if (!(ctx && p_msg && decl)) {
        UEM_WARN("Failed to PopulateDeclarationMetadata due to nullptr! ctx={}, p_msg={}, decl={}", !!ctx, !!p_msg, !!decl);
        return;
    }
    //todo
}

/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(const clang::ASTContext* ctx, Identifier* p_msg, const clang::Decl* decl) {
    if (!(ctx && p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! ctx={}, p_msg={}, decl={}", !!ctx, !!p_msg, !!decl);
        return;
    }
}

/// @brief Finalizes a top-level forward declaration.
inline void FinalizeTL(TLForwardDeclaration* decl, google::protobuf::Arena* arena, std::vector<Declaration*>& container) {
    if (!(decl && arena)) {
        UEM_WARN("Failed to FinalizeTL due to nullptr! TLForwardDeclaration={}, arena={}", !!decl, !!arena);
        return;
    }
    auto decl_container = google::protobuf::Arena::Create<Declaration>(arena);
    decl_container->set_allocated_forward_declaration(decl);
    container.emplace_back(decl_container);
}

/// @brief Finalizes a top-level enum declaration.
inline void FinalizeTL(TLEnumDeclaration* decl, google::protobuf::Arena* arena, std::vector<Declaration*>& container) {
    if (!(decl && arena)) {
        UEM_WARN("Failed to FinalizeTL due to nullptr! TLEnumDeclaration={}, arena={}", !!decl, !!arena);
        return;
    }
    auto decl_container = google::protobuf::Arena::Create<Declaration>(arena);
    decl_container->set_allocated_enum_declaration(decl);
    container.emplace_back(decl_container);
}