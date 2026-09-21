#include "UEMeta/clang/MetaPreprocessor.hpp"

#include "clang/Lex/Lexer.h"
#include "clang/Lex/Token.h"
#include "UEMeta/utility/TokenException.hpp"
#include "UEMeta/clang/ReflectionDb.hpp"

UEMeta::MetaPreprocessor::MetaPreprocessor(clang::SourceManager& sm, clang::LangOptions& opts) : sm(sm), lang_opts(opts) {}

void UEMeta::MetaPreprocessor::MacroExpands(const clang::Token& MacroNameTok, const clang::MacroDefinition& MD, clang::SourceRange Range,
                                            const clang::MacroArgs* Args) {
    ParserTypes::ReflectionKind kind;
    {
        const auto opt_kind = getReflectionKind(MacroNameTok);
        if (!opt_kind) return;
        kind = *opt_kind;
    }

    auto [file, begin_offset] = sm.getDecomposedExpansionLoc(Range.getBegin());
    auto end_loc = sm.getExpansionLoc(Range.getEnd());
    end_loc = clang::Lexer::getLocForEndOfToken(end_loc, 0, sm, lang_opts);
    auto [end_file, end_offset] = sm.getDecomposedLoc(end_loc);

    if (file != end_file) {
        throw TokenException(MacroNameTok, "Macro with reflection kind {} begins and ends in different files!", static_cast<int>(kind));
    }

    ReflectionDb::addReflectionMacro(file, kind, begin_offset, end_offset, sm);
}

std::optional<ParserTypes::ReflectionKind> UEMeta::MetaPreprocessor::getReflectionKind(const clang::Token& MacroNameTok) {
    if (!MacroNameTok.getIdentifierInfo()) return std::nullopt;
    const llvm::StringRef name = MacroNameTok.getIdentifierInfo()->getName();
    if (name == "UCLASS") return ParserTypes::REFLECTION_KIND_UCLASS;
    if (name == "UFUNCTION") return ParserTypes::REFLECTION_KIND_UFUNCTION;
    if (name == "USTRUCT") return ParserTypes::REFLECTION_KIND_USTRUCT;
    if (name == "UENUM") return ParserTypes::REFLECTION_KIND_UENUM;
    if (name == "UPROPERTY") return ParserTypes::REFLECTION_KIND_UPROPERTY;
    if (name == "UINTERFACE") return ParserTypes::REFLECTION_KIND_UINTERFACE;
    if (name.starts_with("DECLARE_DYNAMIC_DELEGATE")) return ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE;
    if (name.starts_with("DECLARE_DYNAMIC_MULTICAST_DELEGATE")) return ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE_MULTICAST;
    if (name.starts_with("DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE")) return ParserTypes::REFLECTION_KIND_DYNAMIC_DELEGATE_MULTICAST_SPARSE;
    return std::nullopt;
}
