#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "CLI/CLI.hpp"
#include "UEMeta/Cli.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

constexpr auto COMPILE_COMMANDS_HELP = "Path to compile_commands.json, or a JSON string representing the "
    "compile_commands.json.";

constexpr auto PREFER_CLANG_HELP = "Use clang-style arguments with the in-process Clang driver.\n"
    "By default, compile_commands uses clang-cl's MSVC-style arguments.";

constexpr auto STRIP_COMMANDS_HELP = "List of compile commands to ignore/strip from compile_commands.\n"
    "PCH related arguments are stripped out by necessity.\n"
    "For compiler arguments that have arguments themselves, you can append '{num_args}' "
    "to the argument to also strip out the next num_args tokens. So, '/I{1}' would strip "
    "out any instances of the /I argument followed by the token immediately after.\n"
    "Argument stripping happens before additional_clang_args are appended.";

constexpr auto ADDITIONAL_CLANG_ARGS_HELP = "List of additional clang args to force into the command list passed to the "
    "in-process Clang driver.\n"
    "-mwaitpkg and -fno-access-control are always added, with /clang: prefixes "
    "in clang-cl mode.";

constexpr auto LOG_HELP = "Path to log file.\nIf empty, no logs will be saved.\nIf given, it should be relative to the "
    "directory of parser.exe, or absolute.";

constexpr auto PATH_BEGIN_HELP = "If given, all file paths in the generated files start at a path_begin item rather "
    "than the file system root.\nThis is useful for stripping PII and eliminating parts of "
    "the path string that aren't needed.\nIf a given path doesn't have a substring in the "
    "path_begin list, then the whole, absolute path is output.";

constexpr auto OUTPUT_DIRECTORY_HELP = "The path to the directory to save generated files in.\nDefaults to Output.";

constexpr auto FORMAT_HELP = "The format of the generated files.\nIf 'binary', then the data will be serialized "
    "according to protobuf's default implementation.\n\tThis is the smallest and fastest format "
    "to parse to and from.\nIf 'json', then the data will be serialized as human-readable JSON."
    "\n\tGood for debugging.";

constexpr auto SYNC_HELP = "When passed, a declaration is saved to a file before moving on to the next file. "
    "This can alleviate memory usage issues, but incurs a performance/time-to-finish penalty.";

constexpr auto PREFER_FULL_NAME_HELP = "By default, the hash of the fully qualified name of a declaration is inserted into"
    " the serialized file name. Specifying this make the fully qualified name of the"
    " declaration appear in the file name, instead of its hash.";

constexpr auto ENABLE_UNREAL_EXTENSIONS_HELP = "Enable Unreal Engine-specific parsing extensions.";

constexpr auto FILE_DELIMITER_HELP = "Directory name that bounds upward file-system searches.";

/**
 * @brief Default compiler arguments removed from Unreal compile command entries before Clang runs.
 */
#define UEM_DEFAULT_STRIP_LIST std::vector<std::string>{"/Yu", "/Fp", "/experimental:log{1}"}
#define UEM_DEFAULT_CLANG_ADDL_ARGS std::vector<std::string>{"-mwaitpkg", "-fno-access-control"}
#define UEM_DEFAULT_CLANG_CL_ADDL_ARGS std::vector<std::string>{"/clang:-mwaitpkg", "/clang:-fno-access-control"}

#ifdef NDEBUG
#define UEM_DEFAULT_FORMAT ::UEMeta::Config::SerializationFormat::Binary
#else
#define UEM_DEFAULT_FORMAT ::UEMeta::Config::SerializationFormat::Json
#endif

/// @brief Returns the normalized compile_commands.json content.
const std::string& UEMeta::Config::getCompileCommands() const {
    assertInitialized();
    return compile_commands;
}

/// @brief Returns arguments appended to the filtered compile command before invoking Clang.
const std::unordered_set<std::string>& UEMeta::Config::getAdditionalClangArgs() const {
    assertInitialized();
    return additional_clang_args;
}

/// @brief Returns compile command arguments stripped before invoking Clang.
const std::unordered_set<std::string>& UEMeta::Config::getStripArgs() const {
    assertInitialized();
    return strip_commands;
}

bool UEMeta::Config::prefersClang() const {
    assertInitialized();
    return prefer_clang;
}

bool UEMeta::Config::prefersFullNameInFileName() const {
    assertInitialized();
    return prefer_full_name_in_file_name;
}

bool UEMeta::Config::syncSerialization() const {
    assertInitialized();
    return sync_serialization;
}

UEMeta::Config::SerializationFormat UEMeta::Config::getFormat() const {
    assertInitialized();
    return format;
}

UEMeta::Config::Mode UEMeta::Config::getMode() const {
    assertInitialized();
    return mode;
}

quill::LogLevel UEMeta::Config::getLogLevel() const {
    assertInitialized();
    return log_level;
}

bool UEMeta::Config::unrealExtensionsEnabled() const {
    assertInitialized();
    return enable_unreal_extensions;
}

const std::filesystem::path::string_type& UEMeta::Config::getFileDelimiter() const {
    assertInitialized();
    return file_delimiter;
}

const UEMeta::StablePath& UEMeta::Config::getLog() {
    assertInitialized();
    return log;
}

const UEMeta::StablePath& UEMeta::Config::getOutputDirectory() const {
    assertInitialized();
    return output_directory;
}

const std::string& UEMeta::Config::getVersion() const {
    assertInitialized();
    return version;
}

/// @brief Returns the process-wide configuration singleton.
UEMeta::Config& UEMeta::Config::getConfig() {
    static Config config{};
    return config;
}

/// @brief Throws if configuration access happens before CLI initialization succeeds.
void UEMeta::Config::assertInitialized() const {
    if (initialized.test())
        return;
    throw std::runtime_error("Tried to use Config before it was initialized!");
}

std::string UEMeta::Config::loadCompileCommandsString(const std::string& in) {
    if (in.ends_with(".json")) {
        std::ifstream ifs{in};
        if (!ifs.is_open()) {
            throw std::runtime_error{fmtquill::format("Could not open compilation database json file {}", in)};
        }
        std::stringstream ss{};
        ss << ifs.rdbuf();
        return ss.str();
    }
    return in;
}

/// @brief Parses CLI arguments and commits validated values into the configuration singleton.
int UEMeta::Config::initialize(int argc, char** argv) {
    auto& cfg = getConfig();
    if (cfg.initialized.test()) {
        UEM_WARN("Tried to initialize an already initialized Config!");
        return 0;
    }

    CLI::App main_app{"Parses C++ code into a mergeable intermediate representation.", "UEMeta"};
    main_app.allow_windows_style_options();
    argv = main_app.ensure_utf8(argv);

    CLI::App* parser = main_app.add_subcommand("parse", "Start the parser as a Clang tool over a translation unit.");
    CLI::App* repl   = main_app.add_subcommand("repl", "Parse strings of C++ code in an interactive command line.");
    main_app.require_subcommand(1);

    // use locals to save per-subcommand common options and resolve them after the fact, since the CLI library doesn't.
    struct ModeOptions {
        StablePath          output_directory{};
        SerializationFormat format{};
        quill::LogLevel      log_level{};
    } parser_options, repl_options;

    const auto try_cli_parse = [&] {
        try {
            main_app.parse(argc, argv);
            cfg.mode = parser->parsed() ? Mode::Parser : Mode::Repl;
        }
        catch (const CLI::CallForHelp& ex) {
            main_app.exit(ex);
            return 0;
        }
        catch (const CLI::ParseError& ex) {
            return main_app.exit(ex);
        }
        catch (const std::exception& ex) {
            UEM_ERROR("CLI parse error: {}", ex.what());
            return -1;
        }
        catch (...) {
            UEM_ERROR("Unknown CLI parse error!");
            return -1;
        }
        return 0;
    };

    parser->add_flag("--prefer-clang", cfg.prefer_clang, PREFER_CLANG_HELP)->default_val(false);
    parser->add_flag("--prefer-full-name-in-file-name", cfg.prefer_full_name_in_file_name, PREFER_FULL_NAME_HELP)->default_val(false);
    parser->add_flag("--sync", cfg.sync_serialization, SYNC_HELP)->default_val(false);
    parser->add_flag("--enable-unreal-extensions", cfg.enable_unreal_extensions, ENABLE_UNREAL_EXTENSIONS_HELP)->default_val(false);
    // todo direct file support
    parser->add_option("--compile-commands", cfg.compile_commands, COMPILE_COMMANDS_HELP)
        ->required()
        ->transform(loadCompileCommandsString);
    parser->add_option("--strip-commands", cfg.strip_commands, STRIP_COMMANDS_HELP)->delimiter(',');
    parser->add_option("--additional-clang-args", cfg.additional_clang_args, ADDITIONAL_CLANG_ARGS_HELP)->delimiter(',');
    parser->add_option("-l,--log", cfg.log, LOG_HELP);
    parser->add_option("--file-delimiter", cfg.file_delimiter, FILE_DELIMITER_HELP)->default_str("UnrealEngine");
    parser->add_option("--output", parser_options.output_directory, OUTPUT_DIRECTORY_HELP)
        ->default_val(StablePath::currentProgramDirectory() / "Output");
    parser->add_option("-f,--format", parser_options.format, FORMAT_HELP)
        ->transform(CLI::CheckedTransformer(string_format_map, CLI::ignore_case))
        ->default_val(UEM_DEFAULT_FORMAT);
    parser->add_option("--log-level", parser_options.log_level)
        ->transform(CLI::CheckedTransformer(string_loglevel_map, CLI::ignore_case))
        ->default_val(quill::LogLevel::Info);

    CLI::Option* output_opt = repl->add_option("--output", repl_options.output_directory);
    repl->add_option("--format", repl_options.format)
        ->transform(CLI::CheckedTransformer(string_format_map, CLI::ignore_case))
        ->default_val(SerializationFormat::Binary)
        ->needs(output_opt);
    repl->add_option("--clang-args", cfg.additional_clang_args)->delimiter(','); // note- must not be CL style args
    repl->add_option("--log-level", repl_options.log_level)
        ->transform(CLI::CheckedTransformer(string_loglevel_map, CLI::ignore_case))
        ->default_val(quill::LogLevel::Error);
    repl->add_option("--version", cfg.version)
        ->default_val("repl");

    if (const auto result = try_cli_parse())
        return result;

    auto& mode_options   = cfg.mode == Mode::Parser ? parser_options : repl_options;
    cfg.output_directory = std::move(mode_options.output_directory);
    cfg.format           = mode_options.format;
    cfg.log_level        = mode_options.log_level;

    if (cfg.mode == Mode::Parser) {
        cfg.strip_commands.insert_range(UEM_DEFAULT_STRIP_LIST);
        cfg.additional_clang_args.insert_range(cfg.prefer_clang ? UEM_DEFAULT_CLANG_ADDL_ARGS : UEM_DEFAULT_CLANG_CL_ADDL_ARGS);
        cfg.version = cfg.output_directory.getUnderlyingPath().filename().string();
    }

    cfg.initialized.test_and_set();
    return 0;
}

/// @brief Returns the initialized Quill logger, falling back to a bootstrap logger during early startup.
quill::Logger* UEMeta::Logger::getQuill() const {
    if (logger)
        return logger;
    std::cerr << "using fallback logger" << std::endl;
    if (auto* fallback_logger = quill::Frontend::get_logger("uemeta_bootstrap")) {
        return fallback_logger;
    }
    quill::Backend::start();
    quill::ConsoleSinkConfig console_sink_config{};
    console_sink_config.set_stream("stderr");
    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("uemeta_bootstrap_console", console_sink_config);
    return quill::Frontend::create_or_get_logger("uemeta_bootstrap", std::move(console_sink));
}

/// @brief Reports whether the main logger sink set has been installed.
bool UEMeta::Logger::isInitialized() const { return !!logger; }

/// @brief Returns the process-wide logger singleton.
UEMeta::Logger& UEMeta::Logger::getLogger() {
    static Logger logger{};
    return logger;
}

/// @brief Throws if code requires the main logger before logger initialization succeeds.
void UEMeta::Logger::assertInitialized() const {
    if (logger)
        return;
    throw std::runtime_error{"Tried to use Logger before it was initialized!."};
}

/// @brief Initializes Quill backend, console/file sinks, and the main logger.
int UEMeta::Logger::initialize() {
    try {
        auto& logger = getLogger();
        auto& cfg    = Config::getConfig();

        quill::Backend::start();
        quill::ConsoleSinkConfig          console_sink_config{};
        quill::ConsoleSinkConfig::Colours colours{};
        quill::PatternFormatterOptions    formatter_options{};
        formatter_options.format_pattern = "%(time) [%(log_level)] %(message)";
        colours.assign_colour_to_log_level(quill::LogLevel::Info, quill::ConsoleSinkConfig::Colours::white);
        console_sink_config.set_colours(colours);
        console_sink_config.set_override_pattern_formatter_options(formatter_options);
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_main", console_sink_config);

        if (auto& log_path = cfg.getLog().getUnderlyingPath(); !log_path.empty()) {
            quill::FileSinkConfig file_sink_config{};
            file_sink_config.set_override_pattern_formatter_options(formatter_options);
            file_sink_config.set_open_mode('w');
            auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_path.string(), file_sink_config);
            if (!console_sink || !file_sink) {
                UEM_ERROR("Failed to initialize logger sinks.");
                return -1;
            }
            logger.logger = quill::Frontend::create_or_get_logger("main", {std::move(console_sink), std::move(file_sink)});
            logger.logger->set_log_level(Config::getConfig().getLogLevel());
            return 0;
        }

        if (!console_sink) {
            UEM_ERROR("Failed to initialize logger sinks.");
            return -1;
        }

        logger.logger = quill::Frontend::create_or_get_logger("main", {std::move(console_sink)});
        logger.logger->set_log_level(Config::getConfig().getLogLevel());

        if (!logger.logger) {
            UEM_ERROR("Failed to initialize logger.");
            return -1;
        }
    }
    catch (const std::exception& ex) {
        UEM_ERROR("Failed to initialize logger with exception: {}", ex.what());
        return -1;
    }
    catch (...) {
        UEM_ERROR("Failed to initialize logger with unknown exception");
        return -1;
    }

    return 0;
}
