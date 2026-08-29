#include "UEMeta/ClangHandler.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/MakeTool.hpp"

/// @brief Initializes logging/configuration, builds the Clang tool, and runs the AST extraction pass.
int main(int argc, char** argv) {
    try {
        if (const auto cfg_init_result = UEMeta::Config::Initialize(argc, argv)) {
            return cfg_init_result;
        }

        if (const auto log_init_result = UEMeta::Logger::Initialize()) {
            return log_init_result;
        }

        UEM_INFO("Using config:\n{}", UEMeta::Config::GetConfig().ToString());

#if defined(DEBUG)
        UEM_INFO("Using debug build of parser! Default output is JSON.");
#endif

        const auto tool = UEMeta::MakeTool();
        if (!tool) return 0;

        switch (UEMeta::RunClangTool(tool->clang_tool)) {
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
