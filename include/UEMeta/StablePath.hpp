#pragma once
#include <filesystem>
#include <ostream>
#include <string>

namespace UEMeta {
    class StablePath {
    public:
        // ReSharper disable once CppNonExplicitConvertingConstructor
        StablePath(std::string_view path, std::error_code& ec) noexcept;

        explicit StablePath(const std::filesystem::path& path, std::error_code& ec) noexcept;

        [[nodiscard]] const std::filesystem::path& UnderlyingPath() const;
        [[nodiscard]] bool Exists() const;
        [[nodiscard]] bool IsFile() const;
        [[nodiscard]] bool IsDirectory() const;
        [[nodiscard]] bool IsEmptyPath() const;
        [[nodiscard]] bool IsEmptyContents() const;

        void Assign(const std::filesystem::path& path, std::error_code& ec) noexcept;
        void Assign(std::string_view path, std::error_code& ec) noexcept;

        friend std::strong_ordering operator<=>(const StablePath& lhs, const StablePath& rhs) noexcept {
            return lhs.path <=> rhs.path;
        }

        friend std::strong_ordering operator<=>(const StablePath& lhs, const std::filesystem::path& rhs) noexcept {
            return lhs.path <=> rhs;
        }

        friend std::strong_ordering operator<=>(const std::filesystem::path& lhs, const StablePath& rhs) noexcept {
            return lhs <=> rhs.path;
        }

        friend std::ostream& operator<<(std::ostream& os, const StablePath& obj) {
            return os << obj.path.string();
        }

        [[nodiscard]] auto begin() const {
            return path.begin();
        }

        [[nodiscard]] auto end() const {
            return path.end();
        }

    private:
        std::filesystem::path path{};
    };
}