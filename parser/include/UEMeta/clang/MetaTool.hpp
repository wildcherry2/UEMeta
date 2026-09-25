#pragma once
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <memory>
#include <string>

namespace UEMeta {
    /**
     * @brief Loads a cached AST or owns the Clang tooling state needed to parse source files.
     */
    class MetaTool {
    public:
        /**
         * @brief Builds the Clang tool when the initialized CLI configuration selects source input.
         *
         * @throws std::runtime_error When compile command loading or validation fails.
         */
        MetaTool();

        /**
         * @brief Extracts metadata from the configured AST cache or source files.
         *
         * @return Clang's run result.
         */
        int runClangTool();

    private:
        /// @brief Removes configured Unreal build arguments before parsing.
        static clang::tooling::CommandLineArguments stripUnneededUnrealBuildArgs(const clang::tooling::CommandLineArguments& args);

        /// @brief Loads, validates, and expands the filtered compile_commands.json content.
        static std::unique_ptr<clang::tooling::CompilationDatabase> loadCompileDatabase(const std::string& cc_json);

        /**
         * @brief Declared before `clang_tool` so the database outlives the tool that references it.
         */
        std::unique_ptr<clang::tooling::CompilationDatabase> compilation_database;

        /**
         * @brief Clang tool configured with the filtered compilation database and source file list.
         */
        std::unique_ptr<clang::tooling::ClangTool> clang_tool;
    };
} // namespace UEMeta
