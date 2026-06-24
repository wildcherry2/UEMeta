#pragma once
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <ranges>
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

constexpr auto STDOUT_HELP = "Overrides all output settings and dumps output to stdout.";

/// @brief Loads a compile_commands.json into a string
static std::string LoadCompileCommandsString(const std::string& in) {
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

#if defined(_WIN32) || defined(WIN32)
/**
 * @brief Default bundled clang-cl executable path on Windows.
 */
#define UEM_DEFAULT_CLANG_CL_PATH UEMeta::StablePath::current_program_directory() / "Clang" / "clang-cl.exe"

/**
 * @brief Default bundled clang executable path on Windows.
 */
#define UEM_DEFAULT_CLANG_PATH UEMeta::StablePath::current_program_directory() / "Clang" / "clang.exe"
#else
/**
 * @brief Default bundled clang-cl executable path on non-Windows platforms.
 */
#define UEM_DEFAULT_CLANG_CL_PATH UEMeta::StablePath::current_program_directory() / "Clang" / "clang-cl"

/**
 * @brief Default bundled clang executable path on non-Windows platforms.
 */
#define UEM_DEFAULT_CLANG_PATH UEMeta::StablePath::current_program_directory() / "Clang" / "clang"
#endif

/**
 * @brief Default compiler arguments removed from Unreal compile command entries before Clang runs.
 */
#define UEM_DEFAULT_STRIP_LIST std::vector<std::string>{"/Yu", "/Fp", "/experimental:log{1}"}
#define UEM_DEFAULT_CLANG_ADDL_ARGS std::vector<std::string>{"-mwaitpkg", "-fno-access-control"}
#define UEM_DEFAULT_CLANG_CL_ADDL_ARGS std::vector<std::string>{"/clang:-mwaitpkg", "/clang:-fno-access-control"}

#ifdef NDEBUG
#define UEM_DEFAULT_FORMAT ::UEMeta::Config::SerializationFormat::json
#else
#define UEM_DEFAULT_FORMAT ::UEMeta::Config::SerializationFormat::json
#endif