#include <sstream>
#include <string_view>
#include <utility>
#include <iostream>

#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/CliHelpers.hpp"
#include "CLI/CLI.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

/// @brief Returns the configured C++ translation unit path.
const UEMeta::StablePath& UEMeta::Config::CppPath() const {
    AssertInitialized();
    return cpp_path;
}

/// @brief Returns the configured compile_commands.json path.
const UEMeta::StablePath& UEMeta::Config::CcPath() const {
    AssertInitialized();
    return cc_path;
}

/// @brief Returns the configured Clang executable path.
const UEMeta::StablePath& UEMeta::Config::ClangPath() const {
    AssertInitialized();
    return clang_path;
}

/// @brief Returns arguments appended to the filtered compile command before invoking Clang.
const std::vector<std::string>& UEMeta::Config::AdditionalClangArgs() const {
    AssertInitialized();
    return additional_clang_args;
}

/// @brief Returns compile command arguments stripped before invoking Clang.
const std::vector<std::string>& UEMeta::Config::StripArgs() const {
    AssertInitialized();
    return strip_args;
}

/// @brief Returns the process-wide configuration singleton.
UEMeta::Config& UEMeta::Config::GetConfig() {
    static Config config{};
    return config;
}

/// @brief Throws if configuration access happens before CLI initialization succeeds.
void UEMeta::Config::AssertInitialized() const {
    if (initialized.test()) return;
    throw std::runtime_error("Tried to use Config before it was initialized!");
}

/// @brief Parses CLI arguments and commits validated values into the configuration singleton.
int UEMeta::Config::Initialize(int argc, char **argv) {
    auto& cfg = GetConfig();
    if (cfg.initialized.test()) {
        UEM_WARN("Tried to initialize an already initialized Config!");
        return 0;
    }

    CLI::App app{"Parses a translation unit with clang tools and outputs a flattened AST of top level declarations.", "UEMeta"};
    app.allow_windows_style_options();
    argv = app.ensure_utf8(argv);

    const auto TryCliParse = [&] {
        try {
            app.parse(argc, argv);
        } catch (const CLI::CallForHelp& ex) {
            app.exit(ex);
            return -1;
        } catch(const CLI::ParseError& ex) {
            return app.exit(ex);
        } catch (const std::exception& ex) {
            UEM_ERROR("CLI parse error: {}", ex.what());
            return -1;
        } catch (...) {
            UEM_ERROR("Unknown CLI parse error!");
            return -1;
        }
        return 0;
    };

    ParsedArgs args{};
    app.add_flag("--prefer-clang", args.prefer_clang, PREFER_CLANG_HELP);
    app.add_option("--compile-commands", args.compile_commands, COMPILE_COMMANDS_HELP)
        ->check(ValidateCompileCommands)->required();
    app.add_option("--clang-path", args.clang_path, CLANG_PATH_HELP)
        ->check(CLI::ExistingFile); // todo if not given, respect --prefer-clang and use bundled binaries
    app.add_option("--strip-commands", args.strip_commands, STRIP_COMMANDS_HELP);
    app.add_option("--clang-args", args.additional_clang_args, ADDITIONAL_CLANG_ARGS_HELP);
    app.add_option("-l,--log", args.log, LOG_HELP);
    app.add_option("--path-begin", args.path_begin, PATH_BEGIN_HELP);
    app.add_option("--output", args.output_directory, OUTPUT_DIRECTORY_HELP);
    app.add_option("-f,--format", args.format, FORMAT_HELP)
        ->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case));

    auto result = TryCliParse();
    if (result) return result;

    try {




        cfg.strip_args.insert_range(cfg.strip_args.end(), UEM_DEFAULT_STRIP_LIST);
        cfg.additional_clang_args.emplace_back("/clang:-mwaitpkg"); //todo get non cl equivalents and use if path is clang
        cfg.additional_clang_args.emplace_back("/clang:-fno-access-control");
        cfg.initialized.test_and_set();
        std::ostringstream config_stream;
        config_stream << cfg;
        UEM_INFO("Using config:\n{}", config_stream.str());
    } catch (const std::exception& ex) {
        UEM_ERROR("Config initialization failed after CLI parse: {}", ex.what());
        return -1;
    } catch (...) {
        UEM_ERROR("Config initialization failed after CLI parse with unknown exception");
        return -1;
    }
    return 0;
}

/// @brief Returns the initialized Quill logger, falling back to a bootstrap logger during early startup.
quill::Logger* UEMeta::Logger::GetQuill() const {
    if (logger) return logger;
    std::cerr << "using fallback logger" << std::endl;
    if (auto* fallback_logger = quill::Frontend::get_logger("uemeta_bootstrap")) {
        return fallback_logger;
    }
    quill::Backend::start();
    quill::ConsoleSinkConfig console_sink_config{};
    console_sink_config.set_stream("stderr");
    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
        "uemeta_bootstrap_console", console_sink_config);
    return quill::Frontend::create_or_get_logger("uemeta_bootstrap", std::move(console_sink));
}

/// @brief Reports whether the main logger sink set has been installed.
bool UEMeta::Logger::IsInitialized() const {
    return !!logger;
}

/// @brief Returns the process-wide logger singleton.
UEMeta::Logger& UEMeta::Logger::GetLogger() {
    static Logger logger{};
    return logger;
}

/// @brief Throws if code requires the main logger before logger initialization succeeds.
void UEMeta::Logger::AssertInitialized() const {
    if (logger) return;
    throw std::runtime_error{"Tried to use Logger before it was initialized!."};
}

/// @brief Initializes Quill backend, console/file sinks, and the main logger.
int UEMeta::Logger::Initialize() {
    try {
        auto& logger = GetLogger();

        quill::Backend::start();
        quill::FileSinkConfig file_sink_config{};
        file_sink_config.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
        quill::ConsoleSinkConfig console_sink_config{};
        quill::ConsoleSinkConfig::Colours colours{};
        quill::PatternFormatterOptions formatter_options{};
        formatter_options.format_pattern = "%(time) [%(log_level)] %(message)";
        colours.assign_colour_to_log_level(quill::LogLevel::Info, quill::ConsoleSinkConfig::Colours::white);
        console_sink_config.set_colours(colours);
        console_sink_config.set_override_pattern_formatter_options(formatter_options);
        file_sink_config.set_override_pattern_formatter_options(formatter_options);
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_main", console_sink_config);
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>("uemeta.log", file_sink_config);

        if (!console_sink || !file_sink) {
            UEM_ERROR("Failed to initialize logger sinks.");
            return -1;
        }

        logger.logger = quill::Frontend::create_or_get_logger("main", {std::move(console_sink), std::move(file_sink)});

        if (!logger.logger) {
            UEM_ERROR("Failed to initialize logger.");
            return -1;
        }

        logger.logger->set_log_level(quill::LogLevel::TraceL1);
    } catch (const std::exception& ex) {
        UEM_ERROR("Failed to initialize logger with exception: {}", ex.what());
        return -1;
    } catch (...) {
        UEM_ERROR("Failed to initialize logger with unknown exception");
        return -1;
    }

    return 0;
}
