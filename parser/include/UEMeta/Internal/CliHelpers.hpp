#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <google/protobuf/util/json_util.h>
#include "parser.pb.h"

constexpr auto COMPILE_COMMANDS_HELP = "Path to compile_commands.json, or a JSON string representing the "
                                       "compile_commands.json.";

constexpr auto PREFER_CLANG_HELP = "Use clang/clang.exe over or clang-cl/clang-cl.exe.\n"
                                   "If this is true, compile_commands must pass clang-compatible commands rather than "
                                   "MSVC-style commands.";

constexpr auto CLANG_PATH_HELP = "Set the path to clang[-cl].exe to use instead of the bundled binaries.\n"
                                 "If this is set, then prefer_clang will be ignored.";

constexpr auto STRIP_COMMANDS_HELP = "List of compile commands to ignore/strip from compile_commands.\n"
                                     "PCH related arguments are stripped out by necessity.\n"
                                     "For compiler arguments that have arguments themselves, you can append '{num_args}' "
                                     "to the argument to also strip out the next num_args tokens. So, '/I{1}' would strip "
                                     "out any instances of the /I argument followed by the token immediately after.\n"
                                     "Argument stripping happens before additional_clang_args are appended.";

constexpr auto ADDITIONAL_CLANG_ARGS_HELP = "List of additional clang args to force into the command list passed to the "
                                       "resolved clang executable.\n"
                                       "/clang:-mwaitpkg and /clang:-fno-access-control are forced to ignore common "
                                       "issues with parsing.";

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


/// @brief Validates that a CLI path names an existing non-empty file, optionally with a required filename.
static std::string ValidateNonEmptyFile(const std::string& path, const std::string& assertFileName = "") {
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

/// @brief Validates a compile commands file or JSON literal using its protobuf shape.
static std::string ValidateCompileCommands(const std::string& in) {
    try {
        if (in.empty()) return "";
        const auto IsValidCCJson = [](const std::string& in) -> std::string {
            ParseResult::CompileCommands compileCommands;
            google::protobuf::util::JsonParseOptions options;
            options.ignore_unknown_fields = true;

            auto status = google::protobuf::util::JsonStringToMessage(in, &compileCommands, options);

            if (!status.ok()) {
                return fmtquill::format("Invalid compile_commands file: {}, error: ", in.c_str(), status.ToString());
            }

            return "";
        };

        if (in.ends_with(".json")) {
            auto test = ValidateNonEmptyFile(in, "compile_commands.json");
            if (!test.empty()) return test;
            std::ifstream ifs{in};
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            return IsValidCCJson(buffer.str());
        }

        return IsValidCCJson(in);
    } catch (const std::exception& e) {
        return fmtquill::format("Invalid compile_commands file (exception): {}", e.what());
    }
}