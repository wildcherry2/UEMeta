#include "UEMeta/JsonHelpers.hpp"

#include <exception>

#include "UEMeta/Cli.hpp"
#include "UEMeta/StablePath.hpp"

/// @brief Normalizes a filesystem path through StablePath and returns its string form.
std::string UEMeta::JsonDetail::StablePathString(const std::filesystem::path& path) {
    return StablePath{path}.string();
}

/// @brief Applies configured path delimiters and blacklist replacements to one output path.
std::string UEMeta::JsonDetail::ScrubFilePath(const std::string_view file) noexcept {
    std::string original_file;
    try {
        original_file = std::string{file};
        std::string out_file = original_file;
        const auto& delimiters = Config::GetConfig().PathDelimiters();
        const auto& blacklist = Config::GetConfig().PathBlacklist();
        bool changed = false;

        if (!delimiters.empty()) {
            size_t highest_delim = std::string::npos;
            for (const auto& delimiter : delimiters) {
                if (delimiter.empty()) {
                    continue;
                }

                const auto delim_loc = out_file.rfind(delimiter);
                if (delim_loc != std::string::npos &&
                    (highest_delim == std::string::npos || delim_loc > highest_delim)) {
                    highest_delim = delim_loc;
                }
            }
            if (highest_delim != std::string::npos) {
                out_file = out_file.substr(highest_delim);
                changed = true;
            }
        }

        if (!blacklist.empty()) {
            constexpr std::string_view replacement = "removed";

            for (const auto& token : blacklist) {
                if (token.empty()) {
                    continue;
                }

                for (auto begin = out_file.find(token);
                     begin != std::string::npos;
                     begin = out_file.find(token, begin + replacement.size())) {
                    out_file.replace(begin, token.size(), replacement);
                    changed = true;
                }
            }
        }

        if (changed) {
            out_file = std::filesystem::path{out_file}.lexically_normal().string();
        }

        return out_file;
    } catch (std::exception& ex) {
        UEM_ERROR("Error scrubbing file {}: '{}', will replace with empty string!", original_file, ex.what());
        return "";
    } catch (...) {
        UEM_ERROR("Unknown error scrubbing file {}, will replace with empty string!", original_file);
        return "";
    }
}

/// @brief Applies ScrubFilePath to a vector of paths while preserving order.
std::vector<std::string> UEMeta::JsonDetail::ScrubFilePaths(const std::vector<std::string>& paths) {
    std::vector<std::string> out;
    out.reserve(paths.size());
    for (const auto& path : paths) {
        out.push_back(ScrubFilePath(path));
    }
    return out;
}
