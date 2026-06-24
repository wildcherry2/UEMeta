#pragma once
#include "UEMeta/Cli.hpp"
#include "parser.pb.h"
#include <clang/AST/ASTContext.h>

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
inline std::string GetDeclAsString(const Data& data, const clang::Decl* decl) {
    std::string s{};
    llvm::raw_string_ostream os(s);
    decl->print(os, data.context->getPrintingPolicy(), 0, true);
    return s;
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

/// @brief Fills out a DeclarationMetadata from a Decl
inline void PopulateDeclarationMetadata(const Data& data, DeclarationMetadata* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateDeclarationMetadata due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    //todo
}

/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(const Data& data, Identifier* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
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