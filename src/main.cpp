#include "UEMeta/ClangHandler.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/MakeTool.hpp"

int main(int argc, char** argv) {
    try {
        if (const auto log_init_result = UEMeta::Logger::Initialize()) {
            return log_init_result;
        }

        if (const auto cfg_init_result = UEMeta::Config::Initialize(argc, argv)) {
            return cfg_init_result;
        }

        const auto tool = UEMeta::MakeTool();
        if (!tool) return 0;

        switch (tool->clang_tool.run(clang::tooling::newFrontendActionFactory<UEMeta::ClangHandler>().get())) {
            case 0: {
                UEM_INFO("Successfully ran tool!");
                return 0;
            }
            case 1: {
                UEM_ERROR("Failed to run tool!");
                return -1;
            }
            default: {
                UEM_WARN("Ran tool on subset of files due to missing compile commands!");
            }
        }
    } catch (std::exception& ex) {
        UEM_ERROR("Exception occurred: {}", ex.what());
        return -1;
    } catch (...) {
        UEM_ERROR("Unknown exception occurred!");
        return -1;
    }
    return 0;
}
