#include <gtest/gtest.h>

#include "parser.pb.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    namespace fs = std::filesystem;

    using ParseResult::TLFileData;

    fs::path SourcePath(const std::string_view filename) {
        return fs::path{UEMETA_TEST_TARGET_INCLUDE_DIR} / filename;
    }

    const std::unordered_map<std::string, TLFileData>& FileInfos() {
        static const auto file_infos = [] {
            std::unordered_map<std::string, TLFileData> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".filebin") {
                    continue;
                }

                std::ifstream input{entry.path(), std::ios::binary};
                TLFileData file_info;
                if (!input || !file_info.ParseFromIstream(&input)) {
                    ADD_FAILURE() << "Failed to parse file-info output " << entry.path();
                    continue;
                }

                const auto path = file_info.path();
                const auto inserted = result.emplace(path, std::move(file_info)).second;
                if (!inserted) {
                    ADD_FAILURE() << "Multiple file-info outputs use path " << path;
                }
            }

            return result;
        }();

        return file_infos;
    }

    const TLFileData& FileInfoFor(const fs::path& source_path) {
        const auto& file_infos = FileInfos();
        const auto found = file_infos.find(source_path.string());
        EXPECT_NE(found, file_infos.end()) << "No file-info output for " << source_path;
        if (found == file_infos.end()) {
            static const TLFileData missing;
            return missing;
        }

        return found->second;
    }

    struct DeclarationIdentity {
        std::string file_path;
        std::uint64_t qualified_name_hash;
    };

    struct FileDeclarationHashes {
        std::vector<std::uint64_t> defined;
        std::vector<std::uint64_t> forward_declared;
    };

    template <typename DeclarationType>
    std::optional<DeclarationIdentity> ReadDeclarationIdentity(const fs::path& path) {
        std::ifstream input{path, std::ios::binary};
        DeclarationType declaration;
        if (!input || !declaration.ParseFromIstream(&input)) {
            ADD_FAILURE() << "Failed to parse declaration output " << path;
            return std::nullopt;
        }

        if (!declaration.has_metadata() || !declaration.metadata().has_identifier()) {
            ADD_FAILURE() << "Declaration output has no identifier metadata " << path;
            return std::nullopt;
        }

        const auto& identifier = declaration.metadata().identifier();
        return DeclarationIdentity{identifier.file_path(), identifier.qualified_name_hash()};
    }

    std::optional<DeclarationIdentity> ReadDeclarationIdentity(const fs::path& path) {
        const auto extension = path.extension();
        if (extension == ".aliasbin") {
            return ReadDeclarationIdentity<ParseResult::TLAliasDeclaration>(path);
        }
        if (extension == ".enumbin") {
            return ReadDeclarationIdentity<ParseResult::TLEnumDeclaration>(path);
        }
        if (extension == ".functionbin") {
            return ReadDeclarationIdentity<ParseResult::TLFreeFunctionDeclaration>(path);
        }
        if (extension == ".fwdeclbin") {
            return ReadDeclarationIdentity<ParseResult::TLForwardDeclaration>(path);
        }
        if (extension == ".varbin") {
            return ReadDeclarationIdentity<ParseResult::TLGlobalVariableDeclaration>(path);
        }
        if (extension == ".classbin" || extension == ".structbin" || extension == ".unionbin") {
            return ReadDeclarationIdentity<ParseResult::TLRecordDeclaration>(path);
        }

        return std::nullopt;
    }

    const std::unordered_map<std::string, FileDeclarationHashes>& DeclarationHashesByPath() {
        static const auto declaration_hashes = [] {
            std::unordered_map<std::string, FileDeclarationHashes> result;

            for (const auto& entry : fs::directory_iterator{fs::path{UEMETA_TEST_OUTPUT_DIR}}) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto identity = ReadDeclarationIdentity(entry.path());
                if (!identity) {
                    continue;
                }

                auto& hashes = result[identity->file_path];
                auto& destination = entry.path().extension() == ".fwdeclbin"
                    ? hashes.forward_declared
                    : hashes.defined;
                destination.push_back(identity->qualified_name_hash);
            }

            return result;
        }();

        return declaration_hashes;
    }

    const FileDeclarationHashes& DeclarationHashesFor(const fs::path& source_path) {
        const auto& declaration_hashes = DeclarationHashesByPath();
        const auto found = declaration_hashes.find(source_path.string());
        EXPECT_NE(found, declaration_hashes.end()) << "No declaration output for " << source_path;
        if (found == declaration_hashes.end()) {
            static const FileDeclarationHashes missing;
            return missing;
        }

        return found->second;
    }

    void ExpectHashes(
        const google::protobuf::RepeatedField<std::uint64_t>& actual,
        std::vector<std::uint64_t> expected_hashes) {
        std::vector<std::uint64_t> actual_hashes{actual.begin(), actual.end()};
        std::ranges::sort(actual_hashes);
        std::ranges::sort(expected_hashes);
        EXPECT_EQ(actual_hashes, expected_hashes);
    }
}

TEST(FileInfoTests, AliasTypes) {
    const auto source_path = SourcePath("AliasTypes.hpp");
    const auto& file_info = FileInfoFor(source_path);
    const auto& declaration_hashes = DeclarationHashesFor(source_path);

    EXPECT_EQ(file_info.path(), source_path.string());
    EXPECT_EQ(file_info.file_occurrence(), 0);
    ASSERT_EQ(declaration_hashes.defined.size(), 14);
    ASSERT_EQ(declaration_hashes.forward_declared.size(), 1);
    ExpectHashes(file_info.defined_type_hashes(), declaration_hashes.defined);
    ExpectHashes(file_info.forward_declaration_hashes(), declaration_hashes.forward_declared);
}

TEST(FileInfoTests, EnumTypes) {
    const auto source_path = SourcePath("EnumTypes.hpp");
    const auto& file_info = FileInfoFor(source_path);
    const auto& declaration_hashes = DeclarationHashesFor(source_path);

    EXPECT_EQ(file_info.path(), source_path.string());
    EXPECT_EQ(file_info.file_occurrence(), 1);
    ASSERT_EQ(declaration_hashes.defined.size(), 26);
    ASSERT_TRUE(declaration_hashes.forward_declared.empty());
    ExpectHashes(file_info.defined_type_hashes(), declaration_hashes.defined);
    ExpectHashes(file_info.forward_declaration_hashes(), declaration_hashes.forward_declared);
}

TEST(FileInfoTests, ForwardDeclarationTypes) {
    const auto source_path = SourcePath("ForwardDeclarationTypes.hpp");
    const auto& file_info = FileInfoFor(source_path);
    const auto& declaration_hashes = DeclarationHashesFor(source_path);

    EXPECT_EQ(file_info.path(), source_path.string());
    EXPECT_EQ(file_info.file_occurrence(), 2);
    ASSERT_EQ(declaration_hashes.defined.size(), 12);
    ASSERT_EQ(declaration_hashes.forward_declared.size(), 11);
    ExpectHashes(file_info.defined_type_hashes(), declaration_hashes.defined);
    ExpectHashes(file_info.forward_declaration_hashes(), declaration_hashes.forward_declared);
}
