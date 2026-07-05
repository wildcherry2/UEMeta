#include <gtest/gtest.h>

#include <filesystem>

namespace {
    namespace fs = std::filesystem;

    fs::path TargetDirectory() {
        return fs::path{UEMETA_TEST_TARGET_DIR};
    }
}

TEST(TargetLayout, HasSingleTranslationUnitAndHeaders) {
    EXPECT_TRUE(fs::exists(fs::path{UEMETA_TEST_TARGET_SOURCE}));
    EXPECT_TRUE(fs::exists(TargetDirectory() / "include" / "AliasTypes.hpp"));
    EXPECT_TRUE(fs::exists(TargetDirectory() / "include" / "EnumTypes.hpp"));
    EXPECT_TRUE(fs::exists(TargetDirectory() / "include" / "FunctionTypes.hpp"));
    EXPECT_TRUE(fs::exists(TargetDirectory() / "include" / "RecordTypes.hpp"));
}
