#pragma once
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <memory>

namespace UEMeta {
    /**
     * @brief Owns the Clang tooling state needed to parse the configured translation unit.
     */
    struct ToolData {
        /**
         * @brief Clang tool configured with the filtered compilation database and source file list.
         */
        clang::tooling::ClangTool clang_tool;

        /**
         * @brief Compilation database backing `clang_tool`; kept alive for the tool's lifetime.
         */
        std::unique_ptr<clang::tooling::CompilationDatabase> compilation_database;
    };

    /**
     * @brief Builds the Clang tool from the initialized CLI configuration.
     *
     * @return Tool data on success, or nullptr when compile command loading or tool setup fails.
     */
    std::unique_ptr<ToolData> MakeTool();
}
