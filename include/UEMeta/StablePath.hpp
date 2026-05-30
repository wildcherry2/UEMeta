#pragma once
#include <compare>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

namespace UEMeta {
    class StablePath {
    public:
        StablePath() = default;

        // ReSharper disable once CppNonExplicitConvertingConstructor
        StablePath(std::string_view path, std::error_code& ec) noexcept;

        explicit StablePath(const std::filesystem::path& path, std::error_code& ec) noexcept;

        [[nodiscard]] const std::filesystem::path& UnderlyingPath() const noexcept;
        [[nodiscard]] std::string string() const;
        [[nodiscard]] bool Exists(std::error_code& ec) const noexcept;
        [[nodiscard]] bool IsFile(std::error_code& ec) const noexcept;
        [[nodiscard]] bool IsDirectory(std::error_code& ec) const noexcept;
        [[nodiscard]] bool IsEmptyPath() const noexcept;
        [[nodiscard]] bool IsEmptyContents(std::error_code& ec) const noexcept;

        void Assign(const std::filesystem::path& path, std::error_code& ec) noexcept;
        void Assign(std::string_view path, std::error_code& ec) noexcept;

        friend std::strong_ordering operator<=>(const StablePath& lhs, const StablePath& rhs) noexcept {
            return lhs.path <=> rhs.path;
        }

        friend std::strong_ordering operator<=>(const StablePath& lhs, const std::filesystem::path& rhs) noexcept {
            std::error_code ec{};
            return lhs <=> StablePath(rhs, ec);
        }

        friend std::strong_ordering operator<=>(const std::filesystem::path& lhs, const StablePath& rhs) noexcept {
            std::error_code ec{};
            return StablePath(lhs, ec) <=> rhs.path;
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
