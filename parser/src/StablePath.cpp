#include "UEMeta/StablePath.hpp"
#include "UEMeta/Cli.hpp"

/// @brief Best-effort logging for failures that happen while normalizing a StablePath.
static inline void LogStablePathFailure(const std::filesystem::path& raw_path, const std::string_view error) noexcept {
    try {
        UEM_ERROR("Failed to construct stable path for '{}': {}", raw_path.string(), error);
    } catch (...) {
    }
}

/// @brief Constructs a stable path from raw path text.
UEMeta::StablePath::StablePath(const std::string_view raw_path) noexcept : StablePath(std::filesystem::path(raw_path)) {}

/// @brief Constructs a stable path from a string.
UEMeta::StablePath::StablePath(const std::string& path) noexcept : StablePath(std::filesystem::path(path)){
}

/// @brief Constructs a stable path from a filesystem path.
UEMeta::StablePath::StablePath(const std::filesystem::path& raw_path) noexcept {
    Assign(raw_path);
}

/// @brief Assigns and normalizes a filesystem path without throwing.
void UEMeta::StablePath::Assign(const std::filesystem::path& raw_path) noexcept {
    try {
        // if the path is empty, it's stable
        if (raw_path.empty()) {
            path = raw_path;
        }
        else {
            // try to convert to absolute canonical, will fail if it doesn't exist
            path = std::filesystem::canonical(raw_path, last_error);
            if (last_error) {
                last_error.clear();
                // try to convert to weakly absolute canonical if normal failed, can fail for OS reasons
                path = std::filesystem::weakly_canonical(raw_path, last_error);
                if (last_error) {
                    last_error.clear();
                    // when all else fails, just make it lexically normal
                    path = raw_path.lexically_normal();
                }
            }
            path = path.lexically_normal();
            path.make_preferred();
        }
    } catch (std::exception& e) {
        LogStablePathFailure(raw_path, e.what());
    } catch (...) {
        LogStablePathFailure(raw_path, "unknown exception!");
    }
}

/// @brief Assigns and normalizes raw path text without throwing.
void UEMeta::StablePath::Assign(std::string_view raw_path) noexcept {
    return Assign(std::filesystem::path(raw_path));
}

/// @brief Returns the normalized filesystem path object.
const std::filesystem::path & UEMeta::StablePath::UnderlyingPath() const noexcept {
    return path;
}

/// @brief Returns the normalized path as a string.
std::string UEMeta::StablePath::string() const {
    return path.string();
}

/// @brief Checks whether the normalized path exists.
bool UEMeta::StablePath::Exists(std::error_code& ec) const noexcept {
    return std::filesystem::exists(path, ec);
}

/// @brief Checks whether the normalized path is a regular file.
bool UEMeta::StablePath::IsFile(std::error_code& ec) const noexcept {
    return std::filesystem::is_regular_file(path, ec);
}

/// @brief Checks whether the normalized path is a directory.
bool UEMeta::StablePath::IsDirectory(std::error_code& ec) const noexcept {
    return std::filesystem::is_directory(path, ec);
}

/// @brief Checks whether the stored path itself is empty.
bool UEMeta::StablePath::IsEmptyPath() const noexcept {
    return path.empty();
}

/// @brief Checks whether the filesystem entry at the normalized path is empty.
bool UEMeta::StablePath::IsEmptyContents(std::error_code& ec) const noexcept {
    return std::filesystem::is_empty(path, ec);
}
