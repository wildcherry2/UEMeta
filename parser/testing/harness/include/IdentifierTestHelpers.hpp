#pragma once

#include <gtest/gtest.h>

#include "VersionedProtoTestHelpers.hpp"

#include <cstdint>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UEMeta::Testing {
    inline bool IsCanonicalWordCharacter(const char character) {
        return character == '_' || character == '$' || character == '\''
            || std::isalnum(static_cast<unsigned char>(character));
    }

    inline std::string CanonicalSpelling(const std::string_view spelling) {
        std::string result;
        result.reserve(spelling.size());
        bool saw_space = false;
        for (std::size_t index = 0; index < spelling.size(); ++index) {
            const auto character = spelling[index];
            if (std::isspace(static_cast<unsigned char>(character))) {
                saw_space = !result.empty();
                continue;
            }

            if (saw_space && !result.empty() && IsCanonicalWordCharacter(result.back())
                && IsCanonicalWordCharacter(character)) {
                result += ' ';
            }
            result += character;
            saw_space = false;
        }

        constexpr std::string_view clang_bool = "_Bool";
        constexpr std::string_view canonical_bool = "bool";
        std::size_t position = 0;
        while ((position = result.find(clang_bool, position)) != std::string::npos) {
            const bool begins_token = position == 0 || !IsCanonicalWordCharacter(result[position - 1]);
            const auto token_end = position + clang_bool.size();
            const bool ends_token = token_end == result.size() || !IsCanonicalWordCharacter(result[token_end]);
            if (begins_token && ends_token) {
                result.replace(position, clang_bool.size(), canonical_bool);
                position += canonical_bool.size();
            } else {
                position = token_end;
            }
        }
        return result;
    }

    inline std::string EffectiveTemplatePrefix(const std::string_view owner_qualified_name) {
        const auto segment_begin = owner_qualified_name.rfind("::");
        const auto segment = owner_qualified_name.substr(
            segment_begin == std::string_view::npos ? 0 : segment_begin + 2);
        if (!segment.starts_with("template<")) {
            return {};
        }
        std::size_t depth = 0;
        for (auto index = std::string_view{"template"}.size(); index < segment.size(); ++index) {
            if (segment[index] == '<') {
                ++depth;
            } else if (segment[index] == '>' && --depth == 0) {
                return std::string{segment.substr(0, index + 1)};
            }
        }
        return {};
    }

    namespace Detail {
        inline std::string_view Trim(const std::string_view value) {
            const auto begin = value.find_first_not_of(" \t\n\r");
            if (begin == std::string_view::npos) {
                return {};
            }
            const auto end = value.find_last_not_of(" \t\n\r");
            return value.substr(begin, end - begin + 1);
        }

        inline std::size_t MatchingAngleBracket(const std::string_view value, const std::size_t open) {
            std::size_t depth = 0;
            for (auto index = open; index < value.size(); ++index) {
                if (value[index] == '<') {
                    ++depth;
                } else if (value[index] == '>' && --depth == 0) {
                    return index;
                }
            }
            return std::string_view::npos;
        }

        inline std::string UndecoratedScopeSegment(std::string_view segment) {
            segment = Trim(segment);
            if (segment.starts_with("template<")) {
                const auto close = MatchingAngleBracket(segment, std::string_view{"template"}.size());
                if (close != std::string_view::npos) {
                    segment.remove_prefix(close + 1);
                }
            } else if (segment.starts_with("template ")) {
                segment.remove_prefix(std::string_view{"template "}.size());
            }

            std::size_t angle_depth = 0;
            for (std::size_t index = 0; index < segment.size(); ++index) {
                const auto character = segment[index];
                if (character == '<') {
                    if (angle_depth == 0) {
                        segment = segment.substr(0, index);
                        break;
                    }
                    ++angle_depth;
                } else if (character == '>') {
                    --angle_depth;
                } else if (character == '(' && index != 0 && angle_depth == 0) {
                    segment = segment.substr(0, index);
                    break;
                }
            }
            return std::string{Trim(segment)};
        }

        inline std::vector<std::string> UndecoratedScope(const std::string_view qualified_name) {
            std::vector<std::string> result;
            std::size_t segment_begin = 0;
            std::size_t angle_depth = 0;
            std::size_t parenthesis_depth = 0;
            std::size_t bracket_depth = 0;

            const auto add_segment = [&](const std::size_t segment_end) {
                const auto undecorated = UndecoratedScopeSegment(
                    qualified_name.substr(segment_begin, segment_end - segment_begin));
                if (!undecorated.empty()) {
                    result.push_back(undecorated);
                }
            };

            for (std::size_t index = 0; index < qualified_name.size(); ++index) {
                switch (qualified_name[index]) {
                case '<':
                    ++angle_depth;
                    break;
                case '>':
                    --angle_depth;
                    break;
                case '(':
                    ++parenthesis_depth;
                    break;
                case ')':
                    --parenthesis_depth;
                    break;
                case '[':
                    ++bracket_depth;
                    break;
                case ']':
                    --bracket_depth;
                    break;
                case ':':
                    if (index + 1 < qualified_name.size() && qualified_name[index + 1] == ':'
                        && angle_depth == 0 && parenthesis_depth == 0 && bracket_depth == 0) {
                        add_segment(index);
                        segment_begin = index + 2;
                        ++index;
                    }
                    break;
                default:
                    break;
                }
            }
            add_segment(qualified_name.size());
            return result;
        }
    }

    inline void ExpectIdentifier(
        const ParseResult::Identifier& identifier,
        const std::string_view expected_name,
        const std::string_view expected_qualified_name,
        const std::filesystem::path& expected_file_path,
        const std::optional<std::string_view> expected_documentation = std::nullopt) {
        EXPECT_EQ(identifier.name(), expected_name);
        EXPECT_EQ(VersionedValue(identifier.file_path()), expected_file_path.string());
        EXPECT_TRUE(identifier.has_documentation());
        if (expected_documentation) {
            EXPECT_EQ(VersionedValue(identifier.documentation()), *expected_documentation);
        }
        static_cast<void>(expected_qualified_name);
    }
}
