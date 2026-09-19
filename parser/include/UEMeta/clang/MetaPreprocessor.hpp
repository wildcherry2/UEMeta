#pragma once
#include <optional>
#include "Enums.pb.h"
#include "clang/Lex/PPCallbacks.h"

namespace UEMeta {
    /// @brief Hooks the preprocessor to extract Unreal-specific reflection macros. Only guaranteed to exist while
    /// preprocessing.
    class MetaPreprocessor : public clang::PPCallbacks {
    public:
        explicit MetaPreprocessor(clang::SourceManager& sm, clang::LangOptions& opts);
        void     MacroExpands(const clang::Token& MacroNameTok, const clang::MacroDefinition& MD, clang::SourceRange Range,
                          const clang::MacroArgs* Args) override;

    private:
        clang::SourceManager&                             sm;
        clang::LangOptions&                               lang_opts;


        static std::optional<ParserTypes::ReflectionKind> getReflectionKind(const clang::Token& MacroNameTok);
    };
}
