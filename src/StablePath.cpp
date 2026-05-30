#include <iostream>
#include "UEMeta/StablePath.hpp"

UEMeta::StablePath::StablePath(const std::string_view path, std::error_code& ec) noexcept : StablePath(std::filesystem::path(path), ec) {}

UEMeta::StablePath::StablePath(const std::filesystem::path& path, std::error_code& ec) noexcept {
    Assign(path, ec);
}

void UEMeta::StablePath::Assign(const std::filesystem::path& path, std::error_code& ec) noexcept {
    try {
        // if the path is empty, it's stable
        if (path.empty()) {
            this->path = path;
        }
        else {
            // try to convert to absolute canonical, will fail if it doesn't exist
            this->path = std::filesystem::canonical(path, ec);
            if (ec) {
                ec.clear();
                // try to convert to weakly absolute canonical if normal failed, can fail for OS reasons
                this->path = std::filesystem::weakly_canonical(path, ec);
                if (ec) {
                    ec.clear();
                    // when all else fails, just make it lexically normal
                    this->path = path.lexically_normal();
                }
            }
        }
    } catch (std::exception& e) {
        ec.clear();
        ec.assign(1, std::generic_category());
        std::cerr << std::format("Failed to construct stable path for '{}': {}", path.string(), e.what());
    } catch (...) {
        ec.clear();
        ec.assign(1, std::generic_category());
        std::cerr << std::format("Failed to construct stable path for '{}': unknown exception!", path.string());
    }
}

void UEMeta::StablePath::Assign(std::string_view path, std::error_code& ec) noexcept {
    return Assign(std::filesystem::path(path), ec);
}

const std::filesystem::path & UEMeta::StablePath::UnderlyingPath() const {
    return path;
}

bool UEMeta::StablePath::Exists() const {
    return std::filesystem::exists(path);
}

bool UEMeta::StablePath::IsFile() const {
    return std::filesystem::is_regular_file(path);
}

bool UEMeta::StablePath::IsDirectory() const {
    return std::filesystem::is_directory(path);
}

bool UEMeta::StablePath::IsEmptyPath() const {
    return path.empty();
}

bool UEMeta::StablePath::IsEmptyContents() const {
    return std::filesystem::is_empty(path);
}