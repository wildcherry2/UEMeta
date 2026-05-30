#include "UEMeta/StablePath.hpp"

#include <format>
#include <iostream>

UEMeta::StablePath::StablePath(const std::string_view raw_path, std::error_code& ec) noexcept : StablePath(std::filesystem::path(raw_path), ec) {}

UEMeta::StablePath::StablePath(const std::filesystem::path& raw_path, std::error_code& ec) noexcept {
    Assign(raw_path, ec);
}

void UEMeta::StablePath::Assign(const std::filesystem::path& raw_path, std::error_code& ec) noexcept {
    try {
        ec.clear();
        // if the path is empty, it's stable
        if (raw_path.empty()) {
            path = raw_path;
        }
        else {
            // try to convert to absolute canonical, will fail if it doesn't exist
            path = std::filesystem::canonical(raw_path, ec);
            if (ec) {
                ec.clear();
                // try to convert to weakly absolute canonical if normal failed, can fail for OS reasons
                path = std::filesystem::weakly_canonical(raw_path, ec);
                if (ec) {
                    ec.clear();
                    // when all else fails, just make it lexically normal
                    path = raw_path.lexically_normal();
                }
            }
            path = path.lexically_normal();
            path.make_preferred();
        }
    } catch (std::exception& e) {
        ec.clear();
        ec.assign(1, std::generic_category());
        std::cerr << std::format("Failed to construct stable path for '{}': {}", raw_path.string(), e.what());
    } catch (...) {
        ec.clear();
        ec.assign(1, std::generic_category());
        std::cerr << std::format("Failed to construct stable path for '{}': unknown exception!", raw_path.string());
    }
}

void UEMeta::StablePath::Assign(std::string_view raw_path, std::error_code& ec) noexcept {
    return Assign(std::filesystem::path(raw_path), ec);
}

const std::filesystem::path & UEMeta::StablePath::UnderlyingPath() const noexcept {
    return path;
}

std::string UEMeta::StablePath::string() const {
    return path.string();
}

bool UEMeta::StablePath::Exists(std::error_code& ec) const noexcept {
    return std::filesystem::exists(path, ec);
}

bool UEMeta::StablePath::IsFile(std::error_code& ec) const noexcept {
    return std::filesystem::is_regular_file(path, ec);
}

bool UEMeta::StablePath::IsDirectory(std::error_code& ec) const noexcept {
    return std::filesystem::is_directory(path, ec);
}

bool UEMeta::StablePath::IsEmptyPath() const noexcept {
    return path.empty();
}

bool UEMeta::StablePath::IsEmptyContents(std::error_code& ec) const noexcept {
    return std::filesystem::is_empty(path, ec);
}