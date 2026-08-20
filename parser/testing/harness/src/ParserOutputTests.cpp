#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

namespace {
    namespace fs = std::filesystem;

    fs::path OutputDirectory() {
        return fs::path{UEMETA_TEST_OUTPUT_DIR};
    }

    bool HasBinaryParserOutput(const fs::path& directory) {
        for (const auto& entry : fs::directory_iterator{directory}) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto extension = entry.path().extension().string();
            if (std::string_view{extension}.ends_with("bin")) {
                return true;
            }
        }

        return false;
    }
}

TEST(ParserOutput, RunsParserWithTestingOutputDirectory) {
    ASSERT_TRUE(fs::exists(OutputDirectory()));
    EXPECT_TRUE(fs::exists(fs::path{UEMETA_TEST_PARSER_OUTPUT_MARKER}));
    EXPECT_TRUE(HasBinaryParserOutput(OutputDirectory()));
}
