#pragma once
#include <compare>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

namespace UEMeta {
    /**
     * @brief Filesystem path wrapper that stores a normalized, preferred-separator path.
     *
     * StablePath tries to canonicalize existing paths, falls back to weak canonicalization, and finally
     * falls back to lexical normalization. It is used anywhere generated output needs stable path strings.
     */
    class StablePath {
    public:
        /**
         * @brief Constructs an empty stable path.
         */
        StablePath() = default;

        /**
         * @brief Constructs a stable path from a string view.
         *
         * @param path Raw path text to normalize.
         */
        // ReSharper disable once CppNonExplicitConvertingConstructor
        StablePath(std::string_view path) noexcept;

        /**
         * @brief Constructs a stable path from a string.
         *
         * @param path Raw path text to normalize.
         */
        explicit StablePath(const std::string& path) noexcept;

        /**
         * @brief Constructs a stable path from a filesystem path.
         *
         * @param path Raw filesystem path to normalize.
         */
        explicit StablePath(const std::filesystem::path& path) noexcept;

        /**
         * @brief Returns the normalized filesystem path object.
         *
         * @return Reference to the stored path.
         */
        [[nodiscard]] const std::filesystem::path& getUnderlyingPath() const noexcept;

        /**
         * @brief Returns the normalized path as a string.
         *
         * @return Stored path converted with `std::filesystem::path::string`.
         */
        [[nodiscard]] std::string string() const;

        /**
         * @brief Checks whether the normalized path exists.
         *
         * @param ec Receives any filesystem error.
         * @return True when the path exists.
         */
        [[nodiscard]] bool exists(std::error_code& ec) const noexcept;

        /**
         * @brief Checks whether the normalized path is a regular file.
         *
         * @param ec Receives any filesystem error.
         * @return True when the path names a regular file.
         */
        [[nodiscard]] bool isFile(std::error_code& ec) const noexcept;

        /**
         * @brief Checks whether the normalized path is a directory.
         *
         * @param ec Receives any filesystem error.
         * @return True when the path names a directory.
         */
        [[nodiscard]] bool isDirectory(std::error_code& ec) const noexcept;

        /**
         * @brief Checks whether the stored path is empty.
         *
         * @return True when no path text is stored.
         */
        [[nodiscard]] bool isEmptyPath() const noexcept;

        /**
         * @brief Checks whether the filesystem entry at the normalized path is empty.
         *
         * @param ec Receives any filesystem error.
         * @return True when the path's target is empty.
         */
        [[nodiscard]] bool isEmptyContents(std::error_code& ec) const noexcept;

        /**
         * @brief Replaces the stored path after applying stable normalization.
         *
         * @param path Raw filesystem path to normalize.
         */
        void assign(const std::filesystem::path& path) noexcept;

        /**
         * @brief Replaces the stored path from raw path text.
         *
         * @param path Raw path text to normalize.
         */
        void assign(std::string_view path) noexcept;

        /**
         * @brief Compares two stable paths by their normalized filesystem path values.
         */
        friend std::strong_ordering operator<=>(const StablePath& lhs, const StablePath& rhs) noexcept { return lhs.path <=> rhs.path; }

        /**
         * @brief Compares a stable path with a raw filesystem path after normalizing the raw path.
         */
        friend std::strong_ordering operator<=>(const StablePath& lhs, const std::filesystem::path& rhs) noexcept {
            return lhs.path <=> StablePath(rhs).path;
        }

        /**
         * @brief Compares a raw filesystem path with a stable path after normalizing the raw path.
         */
        friend std::strong_ordering operator<=>(const std::filesystem::path& lhs, const StablePath& rhs) noexcept {
            return StablePath(lhs).path <=> rhs.path;
        }

        /**
         * @brief Writes the normalized path string to an output stream.
         */
        friend std::ostream& operator<<(std::ostream& os, const StablePath& obj) { return os << obj.path.string(); }

        /**
         * @brief Returns an iterator to the first component of the stored path.
         */
        [[nodiscard]] auto begin() const { return path.begin(); }

        /**
         * @brief Returns the sentinel iterator for the stored path components.
         */
        [[nodiscard]] auto end() const { return path.end(); }

        StablePath& operator=(const std::string& other) {
            assign(std::string_view{other});
            return *this;
        }

        StablePath& operator=(const std::string_view& other) {
            assign(other);
            return *this;
        }

        StablePath& operator=(const std::filesystem::path& other) {
            assign(other);
            return *this;
        }

        operator bool() const noexcept { return !last_error; }

        [[nodiscard]] StablePath operator/(const std::string& other) const { return StablePath(path / other); }

        [[nodiscard]] StablePath operator/(const std::string_view& other) const { return StablePath(path / other); }

        [[nodiscard]] StablePath operator/(const char* other) const { return StablePath(path / other); }

        [[nodiscard]] StablePath operator/(const std::filesystem::path& other) const { return StablePath(path / other); }

        static StablePath currentProgramPath() noexcept;
        static StablePath currentProgramDirectory() noexcept;

    private:
        std::filesystem::path path{};
        std::error_code       last_error{};
    };
} // namespace UEMeta
