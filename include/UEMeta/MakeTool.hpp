#pragma once
#include <clang/Tooling/Tooling.h>
#include <memory>

namespace UEMeta {
    struct ToolData {
        clang::tooling::ClangTool clang_tool;
        std::unique_ptr<clang::tooling::JSONCompilationDatabase> compilation_database;
    };

    std::unique_ptr<ToolData> MakeTool();
}