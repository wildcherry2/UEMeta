#include "UEMeta/Cli.hpp"
#include "UEMeta/clang/MetaTool.hpp"

#include <exception>

#include "UEMeta/Repl.hpp"

/// @brief Initializes logging/configuration, builds the Clang tool, and runs the AST extraction pass.
int main(int argc, char** argv) {
    try {
        if (const auto cfg_init_result = UEMeta::Config::initialize(argc, argv); cfg_init_result != UEMeta::Config::InitializationResult::Success) {
            return cfg_init_result == UEMeta::Config::InitializationResult::Help ? 0 : 1;
        }

        if (const auto log_init_result = UEMeta::Logger::initialize()) {
            return log_init_result;
        }

        if (!UEMeta::Config::getConfig().getOutputDirectory().isEmptyPath()) {
            std::filesystem::create_directories(UEMeta::Config::getConfig().getOutputDirectory().getUnderlyingPath());
        }

        if (const UEMeta::Config::Mode mode = UEMeta::Config::getConfig().getMode();
            mode == UEMeta::Config::Mode::Parser_CC || mode == UEMeta::Config::Mode::Parser_Cache) {
            UEM_INFO("Using config:\n{}", UEMeta::Config::getConfig().toString());

#if defined(DEBUG)
            UEM_INFO("Using debug build of parser! Default output is JSON, and files will have their FQNs rather than FQN hashes!");
#endif

            UEMeta::MetaTool tool;

            switch (tool.runClangTool()) {
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
        }
        else {
            UEMeta::Repl::startLoop();
        }
    }
    catch (std::exception& ex) {
        UEM_ERROR("Exception occurred: {}", ex.what());
        return -1;
    }
    catch (...) {
        UEM_ERROR("Unknown exception occurred!");
        return -1;
    }
    return 0;
}
