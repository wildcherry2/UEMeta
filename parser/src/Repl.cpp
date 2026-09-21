#include "UEMeta/Repl.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <ranges>
#include "UEMeta/Cli.hpp"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "UEMeta/clang/DeclDb.hpp"
#include "UEMeta/clang/MetaASTConsumer.hpp"
#include "UEMeta/clang/wrappers/DeclWrapper.hpp"

void UEMeta::Repl::startLoop() {
    while (std::cin) {
        std::cout << "Enter C++ code, then press enter three times to parse.\n" << std::endl;
        int enter_pressed_times = 0;
        std::string line;
        std::string code;

        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                enter_pressed_times++;
                if (enter_pressed_times == 2) {
                    break;
                }
            }
            else {
                enter_pressed_times = 0;
            }
            code += line;
            code += '\n';
        }

        if (code.empty() || std::ranges::all_of(code, [](const unsigned char ch) { return std::isspace(ch);})) {
            if (!std::cin)
                break;
            std::cout << "Skipping empty parse request...\n" << std::endl;
            continue;
        }

        static std::vector<std::string> clang_args = [] -> std::vector<std::string> {
            const std::unordered_set<std::string>& from_cli = Config::getConfig().getAdditionalClangArgs();
            if (from_cli.empty()) {
                return std::vector<std::string>{"-std=c++20", "-fparse-all-comments", "-Wno-missing-declarations", "--target=x86_64-pc-windows-msvc"};
            }
            return std::vector<std::string>{from_cli.begin(), from_cli.end()};
        }();

        std::unique_ptr<clang::ASTUnit> ast_unit = clang::tooling::buildASTFromCodeWithArgs(code, clang_args, "repl.cpp");
        if (!ast_unit) {
            std::cerr << "Failed to construct AST!" << std::endl;
            continue;
        }
        if (ast_unit->getDiagnostics().hasErrorOccurred()) {
            std::cerr << "Failed to construct AST with errors:" << std::endl;
            for (const clang::StoredDiagnostic& diag : ast_unit->storedDiagnostics()) {
                llvm::errs() << diag << "\n";
            }
            continue;
        }

        clang::ASTContext& ast_context = ast_unit->getASTContext();
        MetaASTConsumer consumer("repl.cpp");
        consumer.Initialize(ast_context);
        consumer.HandleTranslationUnit(ast_context);

        DeclDb::awaitPendingSerializations();
        DeclDb::reset();
        Detail::DeclWrapperStatics::resetDeclOccurrences();
    }
}
