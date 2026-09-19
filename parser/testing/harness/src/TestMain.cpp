#include <filesystem>
#include <iostream>
#include <utility>

#include <gtest/gtest.h>
#include "UEMeta/Cli.hpp"
#include "UEMeta/clang/wrappers/DeclWrapper.hpp"
#include "llvm/Support/FileSystem.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    llvm::SmallString<128> temporary_directory;
    if (const auto error = llvm::sys::fs::createUniqueDirectory("uemeta-parser-tests", temporary_directory)) {
        std::cerr << "Cannot create test output directory: " << error.message() << '\n';
        return 1;
    }
    // Each process owns its directory, including concurrently running CTest cases.
    struct OutputDirectory {
        std::filesystem::path path;
        ~OutputDirectory() {
            UEMeta::Detail::DeclWrapperStatics::awaitPendingSerializations();
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } output{temporary_directory.str().str()};
    const auto version_directory = output.path / "test-version";
    std::filesystem::create_directory(version_directory);

    // Occurrences restart at zero, so output names can repeat across tests and
    // --gtest_repeat iterations. Clear only this process's private output folder.
    struct FreshTestOutput : ::testing::EmptyTestEventListener {
        std::filesystem::path directory;
        explicit FreshTestOutput(std::filesystem::path directory) : directory(std::move(directory)) {}
        void OnTestStart(const ::testing::TestInfo&) override {
            UEMeta::Detail::DeclWrapperStatics::awaitPendingSerializations();
            std::filesystem::remove_all(directory);
            std::filesystem::create_directory(directory);
        }
    };
    ::testing::UnitTest::GetInstance()->listeners().Append(new FreshTestOutput{version_directory});

    // Config already grants main bootstrap access. Initialize only the settings
    // these tests need; CLI11's Windows UTF-8 conversion reads the OS command line.
    auto& config              = UEMeta::Config::getConfig();
    config.output_directory   = UEMeta::StablePath{version_directory};
    config.version            = "test-version";
    config.format             = UEMeta::Config::SerializationFormat::Binary;
    config.sync_serialization = true;
    config.initialized.test_and_set();

    return RUN_ALL_TESTS();
}
