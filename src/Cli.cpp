#include <map>
#include <sstream>
#include <string_view>
#include <utility>
#include <atomic>

#include "UEMeta/Cli.hpp"
#include "CLI/CLI.hpp"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"
#include "indicators/progress_spinner.hpp"

static std::map<std::string, UEMeta::FileSplitStrategy> SSMap {
    {"Default", UEMeta::FileSplitStrategy::Default},
    {"ByClass", UEMeta::FileSplitStrategy::ByClass},
    {"ByParentDirectory", UEMeta::FileSplitStrategy::ByParentDirectory},
    {"ByFile", UEMeta::FileSplitStrategy::ByFile},
    {"Monofile", UEMeta::FileSplitStrategy::Monofile},
};

class ConsoleSinkWithSpinner : public quill::ConsoleSink {
public:
    ConsoleSinkWithSpinner(quill::ConsoleSinkConfig const& config = quill::ConsoleSinkConfig{}) : ConsoleSink(config) {}

    void write_log(const quill::MacroMetadata *log_metadata, uint64_t log_timestamp, std::string_view thread_id,
                   std::string_view thread_name, const std::string &process_id, std::string_view logger_name,
                   quill::LogLevel log_level, std::string_view log_level_description, std::string_view log_level_short_code,
                   const std::vector<std::pair<std::string, std::string>> *named_args, std::string_view log_message,
                   std::string_view log_statement) override {

        if (log_level != quill::LogLevel::TraceL1 || !log_metadata || !log_metadata->tags()) {
            if (spinner) {
                std::cout << "\r\33[2K\r" << std::flush;
                ConsoleSink::write_log(log_metadata, log_timestamp, thread_id,thread_name, process_id,
                    logger_name,log_level, log_level_description, log_level_short_code,named_args, log_message,log_statement);

                tick_count = (tick_count + 1) % 8;
                return spinner->set_progress(tick_count);
            }

            return ConsoleSink::write_log(log_metadata, log_timestamp, thread_id,thread_name, process_id,
                logger_name,log_level, log_level_description, log_level_short_code,named_args, log_message,log_statement);
        }

        auto ooo_msg = [] (std::string_view ctrl){ std::cerr << fmtquill::format("Out-of-order spinner control message '{}' received!", ctrl) << std::endl; };
        if (auto tag = std::string_view(log_metadata->tags()); tag.contains(UEM_START_SPINNER_TAG)) {
            if (spinner) return ooo_msg(tag);
            spinner = std::make_unique<indicators::ProgressSpinner>(
                    indicators::option::PostfixText(log_message),
                    indicators::option::ForegroundColor{indicators::Color::white},
                    indicators::option::SpinnerStates{std::vector<std::string>{"|", "/", "-", "\\", "|", "/", "-", "\\"}},
                    indicators::option::FontStyles{std::vector{indicators::FontStyle::bold}},
                    indicators::option::ShowPercentage(false));
            tick_count = 0;
        }
        else if (tag.contains(UEM_TICK_SPINNER_TAG)) {
            if (!spinner) return ooo_msg(tag);
            tick_count = (tick_count + 1) % 8;
            std::cout << '\r' << std::flush;
            spinner->set_progress(tick_count);
        }
        else if (tag.contains(UEM_UPDATE_SPINNER_TAG)) {
            if (!spinner) return ooo_msg(tag);
            spinner->set_option(indicators::option::PostfixText(log_message));
            tick_count = (tick_count + 1) % 8;
            std::cout << '\r' << std::flush;
            spinner->set_progress(tick_count);
        }
        else if (tag.contains(UEM_STOP_SPINNER_TAG)) {
            if (!spinner) return;
            spinner->set_option(indicators::option::PostfixText(log_message));
            std::cout << "\r\33[2K\r" << std::flush;
            spinner->mark_as_completed();
            spinner = nullptr;
        }
    }

private:
    std::unique_ptr<indicators::ProgressSpinner> spinner{};
    size_t tick_count = 0;
};

class FileSinkWithSpinner : public quill::FileSink {
public:
    FileSinkWithSpinner(std::filesystem::path const &filename, quill::FileSinkConfig const& config = quill::FileSinkConfig{})
        : FileSink(filename, config) {}

    void write_log(const quill::MacroMetadata *log_metadata, uint64_t log_timestamp, std::string_view thread_id,
                   std::string_view thread_name, const std::string &process_id, std::string_view logger_name,
                   quill::LogLevel log_level, std::string_view log_level_description, std::string_view log_level_short_code,
                   const std::vector<std::pair<std::string, std::string>> *named_args, std::string_view log_message,
                   std::string_view log_statement) override {

        if (log_level != quill::LogLevel::TraceL1 || !log_metadata || !log_metadata->tags()) {
            return FileSink::write_log(log_metadata, log_timestamp, thread_id,thread_name, process_id,
                logger_name,log_level, log_level_description, log_level_short_code,named_args, log_message,log_statement);
        }
        if (auto tag = std::string_view(log_metadata->tags()).substr(1);
            tag.contains(UEM_TICK_SPINNER_TAG) || tag.contains(UEM_UPDATE_SPINNER_TAG)) return;

        const quill::MacroMetadata metadata{log_metadata->source_location(), log_metadata->caller_function(), log_metadata->message_format(), "", quill::LogLevel::Info, log_metadata->event()};
        return FileSink::write_log(&metadata, log_timestamp, thread_id,thread_name, process_id,
                logger_name, quill::LogLevel::Info, log_level_description, log_level_short_code,named_args, log_message,log_statement);
    }
};

static std::string ValidateFile(const std::string& path, const std::string& assertFileName = "") {
    std::error_code ec{};
    const UEMeta::StablePath temp{std::string_view{path}};
    if (!temp.Exists(ec)) {
        return fmtquill::format("File \"{}\" not found (OS returned error code {})", path, ec.value());
    }
    if (!temp.IsFile(ec)) {
        return fmtquill::format("File \"{}\" is not a regular file (OS returned error code {})", path, ec.value());
    }
    const auto file_size = std::filesystem::file_size(temp.UnderlyingPath(), ec);
    if (ec || !file_size) {
        return fmtquill::format("File \"{}\" is empty (OS returned error code {})", path, ec.value());
    }
    if (!assertFileName.empty() && temp.UnderlyingPath().filename().string() != assertFileName) {
        return fmtquill::format("File \"{}\" is not named \"{}\" (named '{}')!",
            path, assertFileName, temp.UnderlyingPath().filename().string());
    }
    return "";
}

const UEMeta::StablePath& UEMeta::Config::CppPath() const {
    AssertInitialized();
    return cpp_path;
}

const UEMeta::StablePath& UEMeta::Config::CcPath() const {
    AssertInitialized();
    return cc_path;
}

const UEMeta::StablePath& UEMeta::Config::OutPath() const {
    AssertInitialized();
    return out_path;
}

const UEMeta::FileSplitStrategy& UEMeta::Config::SplitStrategy() const {
    AssertInitialized();
    return split_strategy;
}

const std::vector<UEMeta::StablePath>& UEMeta::Config::PdPaths() const {
    AssertInitialized();
    return pd_paths;
}

const UEMeta::StablePath& UEMeta::Config::ClangPath() const {
    AssertInitialized();
    return clang_path;
}

const std::vector<std::string>& UEMeta::Config::AdditionalClangArgs() const {
    AssertInitialized();
    return additional_clang_args;
}

const std::vector<std::string>& UEMeta::Config::StripArgs() const {
    AssertInitialized();
    return strip_args;
}

const std::vector<std::string>& UEMeta::Config::PathDelimiters() const {
    AssertInitialized();
    return path_delimiters;
}

const std::vector<std::string>& UEMeta::Config::PathBlacklist() const {
    AssertInitialized();
    return path_blacklist;
}

const std::vector<std::string> & UEMeta::Config::HeaderBlacklist() const {
    AssertInitialized();
    return header_blacklist;
}

const std::vector<std::string> & UEMeta::Config::HeaderWhitelist() const {
    AssertInitialized();
    return header_whitelist;
}

UEMeta::Config& UEMeta::Config::GetConfig() {
    static Config config{};
    return config;
}

void UEMeta::Config::AssertInitialized() const {
    if (initialized.test()) return;
    throw std::runtime_error("Tried to use Config before it was initialized!");
}

int UEMeta::Config::Initialize(int argc, char **argv) {
    auto& cfg = GetConfig();
    if (cfg.initialized.test()) {
        UEM_WARN("Tried to initialize an already initialized Config!");
        return 0;
    }

    CLI::App app{"Dumps a simplified AST of a translation unit in an Unreal project.", "UEMeta"};
    app.allow_windows_style_options();
    argv = app.ensure_utf8(argv);

    std::string async_error{};
    std::string cpp_path{};
    std::string cc_path{};
    std::string out_path{};
    std::string clang_path{};
    std::vector<std::string> pd_paths{};

    app.add_option("-f,--file", cpp_path, "Path of the cpp file that drives the translation unit to dump.")
        ->required()->check([](const auto& opt) -> std::string {
            return ValidateFile(opt);
        });

    app.add_option("-c,--compile-commands", cc_path, "Path of the compile_commands.json generated by UBT")
        ->required()->check([](const auto& opt) -> std::string {
            return ValidateFile(opt, "compile_commands.json");
        });

    auto opt_ss = app.add_option("--split-strategy", cfg.split_strategy,
        "How output JSONs should be generated:"
        "\n\t\t(Default) ByFile: Generates an AST JSON for every file."
        "\n\t\tByClass: Generates an AST JSON for every significant node (class, enum, struct, constants, free functions, etc)"
        "\n\t\tMonofile: Puts everything in a single JSON file."
        "\n\t\tByParentDirectory: All ASTs generated within directories given in the --parent-directories command go in the same file.")
            ->transform(CLI::CheckedTransformer(SSMap, CLI::ignore_case));

    auto opt_parent_dir = app.add_option("--parent-directories", pd_paths, "Space-separated list of directories to use as groups for ASTs within a JSON.")
        ->check(CLI::ExistingDirectory);

    app.preparse_callback([&](...) {
        if (opt_ss->as<std::string>() == "ByParentDirectory") {
            opt_parent_dir->required();
        }
    });

    app.add_option("-o,--out", out_path, "Directory to output JSON files to.")
        ->required()->check([](const auto& opt) -> std::string {
            const std::filesystem::path temp_path{opt};
            std::error_code ec{};

            if (std::filesystem::exists(temp_path, ec)) {
                if (!std::filesystem::is_directory(temp_path, ec)) {
                    return "Output path is not a directory!";
                }
                return "";
            }

            if (ec) {
                return fmtquill::format("Failed to check output directory existence: {}", ec.message());
            }

            if (!std::filesystem::create_directory(opt, ec)) {
                return fmtquill::format("Failed to create output directory: {}", ec.message());
            }

            return "";
        });

    auto opt_parse_as_linux = app.add_flag("-p,--parse-as-linux", cfg.no_cl,
        "Uses clang instead of clang-cl. Assumes that the --compile-commands is appropriate "
        "for clang. If --clang is given, this option may be ignored.");

    app.add_option("--clang", clang_path,
        "Path to the clang executable to use. If clang-cl is used, UEMeta will assume "
        "--compile-commands points to a MSVC-style build, otherwise if clang is used it will assume it points "
        "to a clang-style build. If not specified, it uses the bundled clang-cl executable, or the clang executable "
        "if --parse-as-linux is specified.")
        ->check(CLI::ExistingFile)->excludes(opt_parse_as_linux);

    app.add_option("--skip-loaded-args", cfg.strip_args,
        "Additional compile arguments to strip out of the compiler arguments loaded from compile_commands.json.\n"
        "PCH-related arguments are stripped out by necessity.\n"
        "For compiler arguments that have arguments themselves, you can append '{num_args}' to the argument to also strip"
        "out the next num_args tokens. So, '/I{1}' would strip out any instances of the /I argument followed by the"
        "token immediately after. Argument stripping happens before --additional-clang-args are appended.");

    app.add_option("--additional-clang-args", cfg.additional_clang_args,
        "Additional arguments to pass to the clang executable.");

    app.add_option("--path-delimiters", cfg.path_delimiters,
        "Highest level roots allowed in paths in generated JSON files. "
        "If a path delimiter is found in a path in the JSON output, every directory/file above it is stripped from the path "
        "string. If multiple delimiters are found, the most specific root is chosen. "
        "This is for protecting PII when releasing JSONs publicly."
        "Defaults: 'Unreal', 'UnrealEngine', 'MetadataHarness' (forced)");

    app.add_option("--path-blacklist", cfg.path_blacklist,
        "Substrings to strip in paths in generated JSON files."
        "Found substrings will be replaced with the string 'removed'. "
        "This is for protecting PII when releasing JSONs publicly.");

    app.add_option("--header-whitelist", cfg.header_whitelist,
        "If a header's path does not contain at least one token in the header-whitelist, the header will "
        "be excluded from JSON generation. If blacklist tokens are also given, the whitelist runs before the blacklist.");

    app.add_option("--header-blacklist", cfg.header_blacklist,
        "If a header's path contains at least one token in the header-blacklist, the header will be "
        "excluded from JSON generation. If whitelist tokens are also given, the whitelist runs before the blacklist.");

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

    try {
        cfg.cpp_path.Assign(std::string_view{cpp_path});
        cfg.cc_path.Assign(std::string_view{cc_path});
        cfg.out_path.Assign(std::string_view{out_path});

        cfg.pd_paths.clear();
        cfg.pd_paths.reserve(pd_paths.size());
        for (const auto& pd_path : pd_paths) {
            cfg.pd_paths.emplace_back(std::string_view{pd_path});
        }

        if (clang_path.empty()) {
            cfg.clang_path.Assign(cfg.no_cl ? UEM_DEFAULT_CLANG_PATH : UEM_DEFAULT_CLANG_CL_PATH);
        } else {
            cfg.clang_path.Assign(std::string_view{clang_path});
        }

        if (cfg.split_strategy == FileSplitStrategy::ByParentDirectory) {
            bool found_non_empty_file = false;
            for (const auto& parent_path : cfg.pd_paths) {
                std::error_code iterator_ec;
                for (std::filesystem::recursive_directory_iterator it{parent_path.UnderlyingPath(), iterator_ec}, end{};
                     it != end;
                     it.increment(iterator_ec)) {
                    if (iterator_ec) {
                        iterator_ec.clear();
                        continue;
                    }

                    std::error_code file_ec;
                    if (!it->is_regular_file(file_ec)) {
                        continue;
                    }

                    const auto size = it->file_size(file_ec);
                    if (!file_ec && size) {
                        found_non_empty_file = true;
                        break;
                    }
                }
                if (found_non_empty_file) break;
            }
            if (!found_non_empty_file) {
                async_error = "Failed to find any non-empty files in the file trees of given --parent-directories!";
            }
        }

        if (!async_error.empty()) {
            return app.exit({"NoContentInParentDirectories", async_error, CLI::ExitCodes::FileError});
        }

        if (const auto clang_error = ValidateFile(cfg.clang_path.string()); !clang_error.empty()) {
            return app.exit({"MissingClang",
                fmtquill::format("Resolved clang path '{}' is not a file!", cfg.clang_path.string()),
                CLI::ExitCodes::FileError});
        }

        cfg.strip_args.insert_range(cfg.strip_args.end(), UEM_DEFAULT_STRIP_LIST);
        cfg.path_delimiters.emplace_back("MetadataHarness");
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

bool UEMeta::Logger::IsInitialized() const {
    return !!logger;
}

UEMeta::Logger& UEMeta::Logger::GetLogger() {
    static Logger logger{};
    return logger;
}

void UEMeta::Logger::AssertInitialized() const {
    if (logger) return;
    throw std::runtime_error{"Tried to use Logger before it was initialized!."};
}

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
        auto console_sink = quill::Frontend::create_or_get_sink<ConsoleSinkWithSpinner>("console_main", console_sink_config);
        auto file_sink = quill::Frontend::create_or_get_sink<FileSinkWithSpinner>("uemeta.log", file_sink_config);

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
