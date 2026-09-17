#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include "UEMeta/wrappers/DeclWrapper.hpp"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Tooling/Tooling.h"

namespace UEMeta::Testing {
    class WrapperTest : public ::testing::Test {
    protected:
        std::shared_ptr<google::protobuf::Arena> arena = std::make_shared<google::protobuf::Arena>();

        void SetUp() override {
            DeclDb::awaitPendingSerializations();
            DeclDb::reset();
        }

        void TearDown() override {
            // Serialization and DeclDb must release AST references before the fixture does.
            DeclDb::awaitPendingSerializations();
            DeclDb::reset();
            asts.clear();
        }

        clang::ASTContext* parseCode(std::string_view code, std::string_view source_file = "wrapper_fixture.cpp") {
            auto ast = clang::tooling::buildASTFromCodeWithArgs(
                std::string{code}, {"-std=c++20", "-fparse-all-comments", "-Wno-missing-declarations", "--target=x86_64-pc-windows-msvc"},
                std::string{source_file});
            if (!ast || ast->getDiagnostics().hasErrorOccurred()) {
                ADD_FAILURE() << "Invalid wrapper fixture:\n" << code;
                return nullptr;
            }
            auto* context = &ast->getASTContext();
            // Several snippets can participate in one test; retain them until TearDown resets DeclDb.
            asts.push_back(std::move(ast));
            return context;
        }

        static std::filesystem::path outputPath(const ParserTypes::DeclarationMetadata& metadata, std::string_view extension) {
            return Config::getConfig().getOutputDirectory().getUnderlyingPath() /
                   (std::to_string(metadata.decl_id().a()) + std::to_string(metadata.decl_id().b()) + "-" +
                    std::to_string(metadata.occurrence_index().versions(0).value()) + "." + std::string{extension});
        }

        static void expectOutput(const std::filesystem::path& path) {
            ASSERT_TRUE(std::filesystem::is_regular_file(path)) << path;
            EXPECT_GT(std::filesystem::file_size(path), 0u);
        }

    private:
        std::vector<std::unique_ptr<clang::ASTUnit>> asts;
    };
} // namespace UEMeta::Testing
