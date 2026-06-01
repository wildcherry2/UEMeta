#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @namespace UEMeta::JsonDetail
 * @brief Shared helpers used by JSON model serialization and AST output code.
 */
namespace UEMeta::JsonDetail {
    /**
     * @brief Returns a read-only span over the full contents of a vector.
     *
     * @tparam T Element type stored in the vector.
     * @param values Vector whose contents should be exposed as a span.
     * @return Span covering every element in @p values.
     */
    template <typename T>
    [[nodiscard]] std::span<const T> SpanOf(const std::vector<T>& values) noexcept {
        return {values.data(), values.size()};
    }

    /**
     * @brief Returns a span over a vector only when it contains at least one element.
     *
     * @tparam T Element type stored in the vector.
     * @param values Vector whose contents should be exposed as a span.
     * @return Empty optional for an empty vector, otherwise a span covering @p values.
     */
    template <typename T>
    [[nodiscard]] std::optional<std::span<const T>> NonEmptySpanOf(const std::vector<T>& values) noexcept {
        if (values.empty()) {
            return std::nullopt;
        }
        return SpanOf(values);
    }

    /**
     * @brief Returns a string view only when the input string is non-empty.
     *
     * @param value String to expose to serializers.
     * @return Empty optional for an empty string, otherwise a view over @p value.
     */
    [[nodiscard]] inline std::optional<std::string_view> NonEmptyString(const std::string& value) noexcept {
        if (value.empty()) {
            return std::nullopt;
        }
        return std::string_view{value};
    }

    /**
     * @brief Returns true as an optional value and omits false values.
     *
     * @param value Boolean value to expose to serializers.
     * @return Empty optional when @p value is false, otherwise true.
     */
    [[nodiscard]] inline std::optional<bool> TrueOnly(const bool value) noexcept {
        if (!value) {
            return std::nullopt;
        }
        return true;
    }

    /**
     * @brief Normalizes a filesystem path through the project's stable path rules.
     *
     * @param path Path to normalize.
     * @return Stable, preferred-separator path string.
     */
    [[nodiscard]] std::string StablePathString(const std::filesystem::path& path);

    /**
     * @brief Scrubs a source file path according to configured delimiters and blacklist tokens.
     *
     * Path scrubbing is used immediately before JSON output so generated metadata can avoid exposing
     * machine-specific or personally identifying directory segments.
     *
     * @param file File path to scrub.
     * @return Scrubbed and lexically normalized file path, or an empty string if scrubbing fails.
     */
    [[nodiscard]] std::string ScrubFilePath(std::string_view file) noexcept;

    /**
     * @brief Scrubs each path in a vector while preserving order.
     *
     * @param paths File paths to scrub.
     * @return Vector containing the scrubbed form of each input path.
     */
    [[nodiscard]] std::vector<std::string> ScrubFilePaths(const std::vector<std::string>& paths);

    /**
     * @brief Glaze custom writer that serializes an object's `file` member after path scrubbing.
     *
     * @tparam T Object type that exposes a `file` data member.
     * @param object Object whose file member should be scrubbed.
     * @return Scrubbed file path.
     */
    template <typename T>
    [[nodiscard]] std::string FileScrubber(const T& object) noexcept {
        return ScrubFilePath(object.file);
    }
}
