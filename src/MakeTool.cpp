#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <simdjson.h>
#include <iostream>
#include <format>

#include "UEMeta/MakeTool.hpp"
#include "UEMeta/Cli.hpp"

using namespace clang::tooling;

std::string FixupCommand(const std::string& cc_path) {
    try {
        simdjson::ondemand::parser p{};
        auto cpp_path = UEMeta::Config::GetConfig().CppPath().string();
        auto json = simdjson::padded_string::load(cc_path);

        if (json.error()) {
            std::cerr << std::format("(simdjson) Failed to load compile commands at \"{}\" with error: {}", cc_path, static_cast<int>(json.error())) << std::endl;
            return "";
        }
        auto document = p.iterate(json.value());
        if (document.error()) {
            std::cerr << std::format("(simdjson) Failed to iterate compile commands at \"{}\" with error: {}", cc_path, static_cast<int>(document.error())) << std::endl;
        }
        for (auto command_obj : document.get_array()) {
            auto object = command_obj.get_object();
            if (object.error()) {
                continue;
            }

            auto file_obj = object.find_field("file");
            if (file_obj.error())
                continue;
            auto file = file_obj.get_string();
            if (file.error())
                continue;
            if (*file != cpp_path)
                continue;

            auto command_field = object.find_field("command");
            if (command_field.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but it's missing a 'command' field!", cpp_path) << std::endl;
                return "";
            }

            auto command = command_field.get_string();
            if (command.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but its 'command' field is not a string!", cpp_path) << std::endl;
                return "";
            }
            auto cmd_start = command->find_first_of('\"');
            if (cmd_start == std::string_view::npos || cmd_start == command->size() - 1) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but it's not long enough!", cpp_path) << std::endl;
                return "";
            }
            auto cmd_end = command->find_first_of('\"', cmd_start + 1);
            if (cmd_end == std::string_view::npos) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but it's not long enough!", cpp_path) << std::endl;
                return "";
            }
            auto keep = command->substr(cmd_end + 1);
            std::string new_command = UEMeta::Config::GetConfig().ClangPath().string() + std::string(keep);

            auto directory_field = object.find_field("directory");
            if (directory_field.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but it's missing a 'directory' field!", cpp_path) << std::endl;
                return "";
            }

            auto output_field = object.find_field("output");
            if (output_field.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but it's missing an 'output' field!", cpp_path) << std::endl;
                return "";
            }

            auto directory = directory_field.get_string();
            if (directory.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but its 'directory' field is not a string!", cpp_path) << std::endl;
                return "";
            }

            auto output = output_field.get_string();
            if (output.error()) {
                std::cerr << std::format("(simdjson) Found command for file \"{}\", but its 'output' field is not a string!", cpp_path) << std::endl;
                return "";
            }

            simdjson::builder::string_builder sb{};
            sb.start_array();
            sb.start_object();
            sb.append_key_value("file", *file);
            sb.append_comma();
            sb.append_key_value("command", new_command);
            sb.append_comma();
            sb.append_key_value("directory", *directory);
            sb.append_comma();
            sb.append_key_value("output", *output);
            sb.end_object();
            sb.end_array();
            if (!sb.validate_unicode()) {
                std::cerr << std::format("(simdjson) Failed to validate unicode for transformed compile_commands from file \"{}\"", cpp_path) << std::endl;
                return "";
            }
            return std::string(*sb.view());
        }
    } catch (std::exception& ex) {
        std::cerr << std::format("(simdjson) Failed to load compile commands at \"{}\" with error: {}", cc_path, ex.what()) << std::endl;
    } catch (...) {
        std::cerr << std::format("(simdjson) Failed to load compile commands at \"{}\" with unknown error!", cc_path) << std::endl;
    }
    return "";
}

std::unique_ptr<UEMeta::ToolData> UEMeta::MakeTool() {
    try {
        std::string error{};
        const auto cc_as_string = FixupCommand(Config::GetConfig().CcPath().string());

        std::unique_ptr<JSONCompilationDatabase> db = JSONCompilationDatabase::loadFromBuffer(cc_as_string, error, JSONCommandLineSyntax::AutoDetect);
        if (!db) {
            std::cerr << std::format("(llvm) Failed to load filtered compile_commands with error \"{}\" and JSON:\n{}", error, cc_as_string) << std::endl;
            return nullptr;
        }

        const auto sources = std::vector{Config::GetConfig().CppPath().string()};
        auto tool = std::make_unique<ToolData>(ClangTool{*db, sources}, std::move(db));

        tool->clang_tool.appendArgumentsAdjuster([](const CommandLineArguments& args,...) {
            CommandLineArguments out{};

            for (llvm::StringRef arg : args) {
                if (arg.starts_with("/Yu") ||
                    arg.starts_with("/Fp") ||
                    arg.starts_with("/Fo") ||
                    arg.starts_with("/Fd") ||
                    arg.starts_with("/Fe")) {
                    continue;
                }

                out.emplace_back(arg);
            }

            return out;
        });

        return tool;
    } catch (std::exception& ex) {
        std::cerr << std::format("(llvm) Failed to make tools with error: {}", ex.what()) << std::endl;
    } catch (...) {
        std::cerr << std::format("(llvm) Failed to make tools with unknown error!") << std::endl;
    }

    return nullptr;
}
