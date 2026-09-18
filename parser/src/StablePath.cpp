#include "UEMeta/StablePath.hpp"
#include "UEMeta/Cli.hpp"

/// @brief Best-effort logging for failures that happen while normalizing a StablePath.
static inline void logStablePathFailure(const std::filesystem::path& raw_path, const std::string_view error) noexcept {
    try {
        UEM_ERROR("Failed to construct stable path for '{}': {}", raw_path.string(), error);
    }
    catch (...) {
    }
}

/// @brief Constructs a stable path from raw path text.
UEMeta::StablePath::StablePath(const std::string_view raw_path) noexcept : StablePath(std::filesystem::path(raw_path)) {}

/// @brief Constructs a stable path from a string.
UEMeta::StablePath::StablePath(const std::string& path) noexcept : StablePath(std::filesystem::path(path)) {}

/// @brief Constructs a stable path from a filesystem path.
UEMeta::StablePath::StablePath(const std::filesystem::path& raw_path) noexcept { assign(raw_path); }

/// @brief Assigns and normalizes a filesystem path without throwing.
void UEMeta::StablePath::assign(const std::filesystem::path& raw_path) noexcept {
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
    }
    catch (std::exception& e) {
        logStablePathFailure(raw_path, e.what());
    }
    catch (...) {
        logStablePathFailure(raw_path, "unknown exception!");
    }
}

/// @brief Assigns and normalizes raw path text without throwing.
void UEMeta::StablePath::assign(std::string_view raw_path) noexcept { return assign(std::filesystem::path(raw_path)); }

/// @brief Returns the normalized filesystem path object.
const std::filesystem::path& UEMeta::StablePath::getUnderlyingPath() const noexcept { return path; }

/// @brief Returns the normalized path as a string.
std::string UEMeta::StablePath::string() const { return path.string(); }

/// @brief Checks whether the normalized path exists.
bool UEMeta::StablePath::exists(std::error_code& ec) const noexcept { return std::filesystem::exists(path, ec); }

/// @brief Checks whether the normalized path is a regular file.
bool UEMeta::StablePath::isFile(std::error_code& ec) const noexcept { return std::filesystem::is_regular_file(path, ec); }

/// @brief Checks whether the normalized path is a directory.
bool UEMeta::StablePath::isDirectory(std::error_code& ec) const noexcept { return std::filesystem::is_directory(path, ec); }

/// @brief Checks whether the stored path itself is empty.
bool UEMeta::StablePath::isEmptyPath() const noexcept { return path.empty(); }

/// @brief Checks whether the filesystem entry at the normalized path is empty.
bool UEMeta::StablePath::isEmptyContents(std::error_code& ec) const noexcept { return std::filesystem::is_empty(path, ec); }

UEMeta::StablePath UEMeta::StablePath::currentProgramDirectory() noexcept {
    auto current_path = currentProgramPath();
    if (current_path && current_path.path.has_parent_path()) {
        return StablePath(current_path.path.parent_path());
    }
    current_path.last_error.assign(1, std::system_category());
    UEM_ERROR("StablePath::currentProgramDirectory failed!");
    return current_path;
}

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
UEMeta::StablePath UEMeta::StablePath::currentProgramPath() noexcept {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH) > 0) {
        if (auto out = StablePath(std::filesystem::path(path)))
            return out;
        else
            UEM_ERROR("StablePath::currentProgramPath failed with error: {}!", out.last_error.value());
    }
    auto out = StablePath();
    out.last_error.assign(GetLastError(), std::system_category());
    UEM_ERROR("StablePath::currentProgramPath failed with error: {}!", out.last_error.value());
    return out;
}

#elif defined(__linux__)
UEMeta::StablePath UEMeta::StablePath::currentProgramPath() noexcept {
    std::error_code ec{};
    auto            path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        auto out       = StablePath();
        out.last_error = ec;
        UEM_ERROR("StablePath::currentProgramPath failed with error: {}!", out.last_error.value());
        return out;
    }
    return StablePath(path);
}

#else
UEMeta::StablePath UEMeta::StablePath::currentProgramPath() noexcept {
    UEM_ERROR("StablePath::currentProgramPath not implemented for platform (WIN32 and __linux__ only!)");
    auto out = StablePath();
    out.last_error.assign(1, std::system_category());
    return out;
}
#endif
