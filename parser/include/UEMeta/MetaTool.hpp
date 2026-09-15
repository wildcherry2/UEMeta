#pragma once
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <memory>
#include <string>

namespace UEMeta {
    /**
     * @brief Owns the Clang tooling state needed to parse the configured translation unit.
     */
    class MetaTool {
    public:
        /**
         * @brief Builds the Clang tool from the initialized CLI configuration.
         *
         * @throws std::runtime_error When compile command loading or validation fails.
         */
        MetaTool();

        /**
         * @brief Runs the Clang tool with UEMeta's AST frontend action.
         *
         * @return Clang's run result.
         */
        int RunClangTool();

    private:
        /// @brief Removes configured Unreal build arguments before parsing.
        static clang::tooling::CommandLineArguments StripUnneededUnrealBuildArgs(
            const clang::tooling::CommandLineArguments& args);

        /// @brief Loads, validates, and expands the filtered compile_commands.json content.
        static std::unique_ptr<clang::tooling::CompilationDatabase> LoadCompileDatabase(const std::string& cc_json);

        /**
         * @brief Declared before `clang_tool` so the database outlives the tool that references it.
         */
        std::unique_ptr<clang::tooling::CompilationDatabase> compilation_database;

        /**
         * @brief Clang tool configured with the filtered compilation database and source file list.
         */
        clang::tooling::ClangTool clang_tool;
    };
}
