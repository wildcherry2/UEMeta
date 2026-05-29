#include "UEMeta/ClangHandler.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/MakeTool.hpp"

int main(int argc, char** argv) {
    try {
        if (const auto cfg_init_result = UEMeta::Config::Initialize(argc, argv)) {
            return cfg_init_result;
        }

        const auto tool = UEMeta::MakeTool();
        if (!tool) return 0;

        switch (tool->clang_tool.run(clang::tooling::newFrontendActionFactory<UEMeta::ClangHandler>().get())) {
            case 0: {
                std::cout << "Successfully ran tool!" << std::endl;
                return 0;
            }
            case 1: {
                std::cout << "Failed to run tool!" << std::endl;
                return -1;
            }
            default: {
                std::cout << "Ran tool on subset of files due to missing compile commands!" << std::endl;
            }
        }
    } catch (std::exception& ex) {
        std::cerr << std::format("Exception occurred: {}", ex.what()) << std::endl;
        return -1;
    } catch (...) {
        std::cerr << std::format("Unknown exception occurred!") << std::endl;
        return -1;
    }
    return 0;
}