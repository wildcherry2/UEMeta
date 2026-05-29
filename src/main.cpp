#include "UEMeta/ClangHandler.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/MakeTool.hpp"

int main(int argc, char** argv) {
    if (const auto cfg_init_result = UEMeta::Config::Initialize(argc, argv)) {
        return cfg_init_result;
    }

    const auto tool = UEMeta::MakeTool();
    if (!tool) return 0;

    const auto run_result = tool->clang_tool.run(clang::tooling::newFrontendActionFactory<UEMeta::ClangHandler>().get());

    return 0;
}