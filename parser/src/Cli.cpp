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


/// @brief Returns the normalized compile_commands.json content.
const std::string& UEMeta::Config::CompileCommands() const {
    AssertInitialized();
    return compile_commands;
}

/// @brief Returns the configured Clang executable path.
const UEMeta::StablePath& UEMeta::Config::ClangPath() const {
    AssertInitialized();
    return clang_path;
}

/// @brief Returns arguments appended to the filtered compile command before invoking Clang.
const std::unordered_set<std::string>& UEMeta::Config::AdditionalClangArgs() const {
    AssertInitialized();
    return additional_clang_args;
}

/// @brief Returns compile command arguments stripped before invoking Clang.
const std::unordered_set<std::string>& UEMeta::Config::StripArgs() const {
    AssertInitialized();
    return strip_commands;
}

bool UEMeta::Config::PrefersClang() const {
    AssertInitialized();
    return prefer_clang;
}

bool UEMeta::Config::DumpToStdout() const {
    AssertInitialized();
    return dump_to_stdout;
}

UEMeta::Config::SerializationFormat UEMeta::Config::Format() const {
    AssertInitialized();
    return format;
}

const std::unordered_set<std::string> & UEMeta::Config::PathBegin() const {
    AssertInitialized();
    return path_begin;
}

const UEMeta::StablePath & UEMeta::Config::Log() {
    AssertInitialized();
    return log;
}

const UEMeta::StablePath & UEMeta::Config::OutputDirectory() const {
    AssertInitialized();
    return output_directory;
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
            return 0;
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

    app.add_flag("--prefer-clang", cfg.prefer_clang, PREFER_CLANG_HELP)
        ->default_val(false);
    app.add_flag("--stdout", cfg.dump_to_stdout, STDOUT_HELP)
        ->default_val(false);
    app.add_option("--compile-commands", cfg.compile_commands, COMPILE_COMMANDS_HELP)
        ->required()
        ->transform(LoadCompileCommandsString);
    app.add_option("--clang-path", cfg.clang_path, CLANG_PATH_HELP)
        ->check(CLI::ExistingFile);
    app.add_option("--strip-commands", cfg.strip_commands, STRIP_COMMANDS_HELP);
    app.add_option("--clang-args", cfg.additional_clang_args, ADDITIONAL_CLANG_ARGS_HELP);
    app.add_option("-l,--log", cfg.log, LOG_HELP);
    app.add_option("--path-begin", cfg.path_begin, PATH_BEGIN_HELP);
    app.add_option("--output", cfg.output_directory, OUTPUT_DIRECTORY_HELP)
        ->default_val(StablePath::current_program_directory() / "Output");
    app.add_option("-f,--format", cfg.format, FORMAT_HELP)
        ->transform(CLI::CheckedTransformer(string_format_map, CLI::ignore_case))
        ->default_val(UEM_DEFAULT_FORMAT);

    if (const auto result = TryCliParse()) return result;

    if (cfg.clang_path.UnderlyingPath().empty()) {
        cfg.clang_path = cfg.prefer_clang ? UEM_DEFAULT_CLANG_PATH : UEM_DEFAULT_CLANG_CL_PATH;
    }

    cfg.strip_commands.insert_range(UEM_DEFAULT_STRIP_LIST);
    cfg.additional_clang_args.insert_range(cfg.prefer_clang ? UEM_DEFAULT_CLANG_ADDL_ARGS : UEM_DEFAULT_CLANG_CL_ADDL_ARGS);

    cfg.initialized.test_and_set();
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
        auto& cfg = Config::GetConfig();

        quill::Backend::start();
        quill::ConsoleSinkConfig console_sink_config{};
        quill::ConsoleSinkConfig::Colours colours{};
        quill::PatternFormatterOptions formatter_options{};
        formatter_options.format_pattern = "%(time) [%(log_level)] %(message)";
        colours.assign_colour_to_log_level(quill::LogLevel::Info, quill::ConsoleSinkConfig::Colours::white);
        console_sink_config.set_colours(colours);
        console_sink_config.set_override_pattern_formatter_options(formatter_options);
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_main", console_sink_config);

        if (auto& log_path = cfg.Log().UnderlyingPath(); !log_path.empty()) {
            quill::FileSinkConfig file_sink_config{};
            file_sink_config.set_override_pattern_formatter_options(formatter_options);
            auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_path.string(), file_sink_config);
            if (!console_sink || !file_sink) {
                UEM_ERROR("Failed to initialize logger sinks.");
                return -1;
            }
            logger.logger = quill::Frontend::create_or_get_logger("main", {std::move(console_sink), std::move(file_sink)});
            logger.logger->set_log_level(quill::LogLevel::TraceL1);
            return 0;
        }

        if (!console_sink) {
            UEM_ERROR("Failed to initialize logger sinks.");
            return -1;
        }

        logger.logger = quill::Frontend::create_or_get_logger("main", {std::move(console_sink)});
        logger.logger->set_log_level(quill::LogLevel::TraceL1);

        if (!logger.logger) {
            UEM_ERROR("Failed to initialize logger.");
            return -1;
        }
    } catch (const std::exception& ex) {
        UEM_ERROR("Failed to initialize logger with exception: {}", ex.what());
        return -1;
    } catch (...) {
        UEM_ERROR("Failed to initialize logger with unknown exception");
        return -1;
    }

    return 0;
}
