#pragma once
#include <algorithm>
#include <execution>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "parser.pb.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "clang/AST/Decl.h"
#include "../Cli.hpp"

std::string ClangToString(clang::ASTContext& context, const clang::Decl* decl);
void PopulateEnumDetails(clang::ASTContext& context, ParseResult::EnumDetails* p_msg, const clang::EnumDecl* decl);
void PopulateDeclarationMetadata(clang::ASTContext& context, ParseResult::DeclarationMetadata* p_msg, const clang::Decl* decl);
void PopulateTemplateDetails(clang::ASTContext& context, ParseResult::TemplateDetails* p_msg, const clang::Decl* decl);

namespace UEMeta {
    class ASTData {
    public:
        [[nodiscard]] clang::ASTContext& GetContext() const {
            if (!context) throw std::runtime_error("Called GetContext before context was initialized!");
            return *context;
        }

        void SetContext(clang::ASTContext* ctx) {
            context = ctx;
        }

        template<typename MessageType>
        [[nodiscard]] MessageType* Allocate() const {
            auto* msg = google::protobuf::Arena::Create<MessageType>(&arena);
            to_serialize.push_back(msg);
            return msg;
        }

        void Invalidate(const google::protobuf::Message* msg) const {
            if (const auto found = std::find(std::execution::par_unseq, to_serialize.begin(), to_serialize.end(), msg); found != to_serialize.end()) {
                to_serialize.erase(found);
            }
        }

        void AddVisitedDecl(const clang::Decl* clang_decl, google::protobuf::Message* msg) {
            visited_decls.insert(std::pair{clang_decl, msg});
            all_unique_visited_decls.push_back(clang_decl);
        }

        [[nodiscard]] bool OnVisit(const auto* decl) {
            if (!decl) return true;

            if (const auto as_tag = llvm::dyn_cast_or_null<clang::TagDecl>(decl)) {
                // if this is a forward declaration...
                if (!as_tag->isThisDeclarationADefinition()) {
                    // and we don't know about it yet...
                    if (!visited_forward_decls.contains(decl)) {
                        // generate a new forward declaration message and return
                        auto& ast_context = GetContext();
                        auto* p_forward_decl = Allocate<ParseResult::TLForwardDeclaration>();
                        PopulateDeclarationMetadata(ast_context, p_forward_decl->mutable_metadata(), decl);

                        // when the forward declaration is templated, we need to explicitly get the template decl
                        const clang::Decl* printable_decl = decl;
                        if (const auto* record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
                            if (const auto* template_decl = record_decl->getDescribedClassTemplate()) {
                                printable_decl = template_decl;
                            }
                        }
                        p_forward_decl->set_as_string(ClangToString(ast_context, printable_decl));

                        // assign the kind and template info
                        switch (as_tag->getTagKind()) {
                            case clang::TagTypeKind::Struct:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_STRUCT);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Union:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_UNION);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Class:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_CLASS);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Enum:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_ENUM);
                                PopulateEnumDetails(ast_context, p_forward_decl->mutable_enum_details(), llvm::dyn_cast_or_null<clang::EnumDecl>(decl));
                                break;
                            default:
                                throw std::runtime_error("AddForwardDeclaration can't be called on a nonstandard declaration!");
                        }

                        visited_forward_decls.insert(std::pair<const clang::Decl*, google::protobuf::Message*>(decl, p_forward_decl));
                        all_unique_visited_decls.push_back(decl);
                        return OnAfterVisit(as_tag, p_forward_decl->metadata().identifier().qualified_name_hash());
                    }
                    return true;
                }
            }
            // if this is a complete definition that we've already seen (secondary translation unit), continue
            if (visited_decls.contains(decl)) return true;

            return false;
        }

        [[nodiscard]] bool OnAfterVisit(const clang::Decl* clang_decl, const uint64_t clang_decl_fqn_hash) const {
            // populate parent nested hashes
            if (const auto decl_context = clang_decl->getDeclContext(); decl_context->isRecord()) {
                auto* as_cls = llvm::cast<clang::RecordDecl>(decl_context);
                if (const auto parent = visited_decls.find(as_cls); parent != visited_decls.end()) {
                    if (auto* parent_record = dynamic_cast<ParseResult::TLRecordDeclaration*>(parent->getSecond())) {
                        parent_record->add_nested_hashes(clang_decl_fqn_hash);
                    }
                }
            }
            logger.Increment();
            return true;
        }

        const std::vector<google::protobuf::Message*>& GetAllMessages() const {
            return to_serialize;
        }

        CountingHeartbeatLogger& GetTraverseLogger() const {
            return logger;
        }

        void GenerateFileData() {
            if (all_unique_visited_decls.empty()) return;

            // sort all declarations by order of appearance
            {
                const auto& sm = context->getSourceManager();
                HeartbeatLogger sort_logger{fmtquill::format("Sorting {} declarations by occurrence...", all_unique_visited_decls.size())};
                sort_logger.Start();
                // sort by occurrence in TU
                std::sort(std::execution::par_unseq, all_unique_visited_decls.begin(), all_unique_visited_decls.end(),
                [&](const clang::Decl* lhs, const clang::Decl* rhs) {
                    return sm.isBeforeInTranslationUnit(lhs->getLocation(), rhs->getLocation());
                });
                sort_logger.Stop();
                UEM_INFO("Sorted {} declarations!", all_unique_visited_decls.size());
            }

            // assign occurrence indices and generate TLFileData
            {
                CountingHeartbeatLogger occ_logger{"Generating TLFileData from declarations ({} declarations remaining)..."};
                occ_logger.SetValue(all_unique_visited_decls.size());
                occ_logger.Start();
                auto* current_file = Allocate<ParseResult::TLFileData>();
                current_file->set_path(GetInfo(all_unique_visited_decls[0]).metadata->identifier().file_path());
                current_file->set_file_occurrence(0);
                current_file->set_path_hash(GetInfo(all_unique_visited_decls[0]).metadata->identifier().file_path_hash());

                uint32_t occurrence_counter = 0;
                for (auto decl_it = all_unique_visited_decls.begin(); decl_it != all_unique_visited_decls.end(); ++decl_it) {
                    auto& info = GetInfo(*decl_it);

                    // switching to a new file, now we need to make sure there are no gaps and patch if there are
                    if (const auto file_hash = info.metadata->identifier().file_path_hash(); file_hash != current_file->path_hash()) {
                        auto subrange = std::ranges::find_last_if(std::next(decl_it), all_unique_visited_decls.end(),
                    [current_file, this](const clang::Decl* decl) {
                            return GetInfo(decl).metadata->identifier().file_path_hash() == current_file->path_hash();
                        });

                        // found a gap, patch it while processing it
                        if (!subrange.empty()) {
                            for (auto end = subrange.begin(); decl_it != end; ++decl_it) {
                                const auto& to_patch_info = GetInfo(*decl_it);
                                to_patch_info.metadata->mutable_identifier()->set_file_path(current_file->path());
                                to_patch_info.metadata->mutable_identifier()->set_file_path_hash(current_file->path_hash());
                                to_patch_info.metadata->set_occurrence_index(occurrence_counter++);
                                occ_logger.Decrement();
                            }
                            info = GetInfo(*decl_it);
                            // fall through to fill out occurrence index for current decl_it
                        }

                        // no gaps, update file stuff and reset occurrence counter
                        else {
                            const auto occ = current_file->file_occurrence() + 1;
                            current_file = Allocate<ParseResult::TLFileData>();
                            current_file->set_path(info.metadata->identifier().file_path());
                            current_file->set_file_occurrence(occ);
                            current_file->set_path_hash(info.metadata->identifier().file_path_hash());
                            occurrence_counter = 0;
                        }
                    }

                    info.metadata->set_occurrence_index(occurrence_counter++);
                    occ_logger.Decrement();
                }

                occ_logger.Stop();
                UEM_INFO("Generated {} TLFileData!", current_file ? current_file->file_occurrence() + 1 : 0);
            }
        }

        void Serialize() {
            logger.SetStr("Serializing {} items...");
            logger.SetValue(to_serialize.size());
            logger.Start();
            constexpr google::protobuf::json::PrintOptions options {.add_whitespace = true, .always_print_fields_with_no_presence = true };
            const auto& cfg = Config::GetConfig();
            if (cfg.DumpToJson()) {
                std::string buffer{};
                const auto wrapper = google::protobuf::Arena::Create<ParseResult::TLItemList>(&arena);
                const auto list = wrapper->mutable_items();
                for (auto* msg : to_serialize) {
                    auto item = google::protobuf::Arena::Create<ParseResult::TLItem>(&arena);
                    dynamic_cast<ParseResult::TLRecordDeclaration*>(msg) ? item->set_allocated_record(static_cast<ParseResult::TLRecordDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLEnumDeclaration*>(msg) ? item->set_allocated_enum_declaration(static_cast<ParseResult::TLEnumDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLForwardDeclaration*>(msg) ? item->set_allocated_forward_declaration(static_cast<ParseResult::TLForwardDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLAliasDeclaration*>(msg) ? item->set_allocated_alias(static_cast<ParseResult::TLAliasDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLFreeFunctionDeclaration*>(msg) ? item->set_allocated_function(static_cast<ParseResult::TLFreeFunctionDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLGlobalVariableDeclaration*>(msg) ? item->set_allocated_variable(static_cast<ParseResult::TLGlobalVariableDeclaration*>(msg))
                        : dynamic_cast<ParseResult::TLFileData*>(msg) ? item->set_allocated_file_data(static_cast<ParseResult::TLFileData*>(msg))
                        : [&]{ UEM_WARN("Unsupported decl, dropping: {}", msg->DebugString()); }();
                    list->AddAllocated(item);
                    logger.Decrement();
                }

                if (google::protobuf::util::MessageToJsonString(*wrapper, &buffer, options).ok()) {
                    const auto& log_path = Config::GetConfig().Log().UnderlyingPath();
                    const auto out_path = (log_path.empty() ? StablePath::current_program_directory().UnderlyingPath()
                        : log_path.parent_path()) / "dump.json";
                    UEM_INFO("Dumping to {}", out_path.string());
                    std::ofstream json_file(out_path, std::ios::trunc);
                    json_file << buffer;
                    json_file.close();
                }
                else {
                    UEM_ERROR("Failed to serialize item list to JSON!");
                }
                return;
            }

            const auto out_dir = cfg.OutputDirectory().UnderlyingPath();
            const auto is_json = cfg.Format() == Config::SerializationFormat::json;

            // Actually generates the file
            const auto GenerateOutFile = [&](google::protobuf::Message* msg) {
                logger.Decrement();
                if (!msg) return;

                const auto GetOutData = [&] () -> std::pair<std::filesystem::path, google::protobuf::Message*> {
                    if (const auto* p_file = dynamic_cast<ParseResult::TLFileData*>(msg)) {
                        // file path is in format {filepathhash}-{fileoccurrenceindex}.file[bin|json]
                        return {out_dir / fmtquill::format("{}-{}.file{}",
                            p_file->path_hash(), p_file->file_occurrence(), is_json ? "json" : "bin"), msg};
                    }

                    // file path is in format {qualnamehash}-{contenthash}-{filepathhash}-{occurrenceindex}.[class|struct|enum|union|alias|function|fwdecl|var][bin|json]
                    DeclInfo* info{};
                    try {
                        info = &GetInfo(msg);
                    } catch (const std::exception& ex) {
                        UEM_WARN("Skipping unsupported serialization message: {}", ex.what());
                        return std::pair<std::filesystem::path, google::protobuf::Message*>{};
                    }

                    const auto& ident = info->metadata->identifier();
                    return {out_dir / fmtquill::format("{}-{}-{}-{}.{}{}", ident.qualified_name_hash(),
                        info->metadata->content_hash(), ident.file_path_hash(), info->metadata->occurrence_index(),
                        info->extension, is_json ? "json" : "bin"), info->message};
                };

                const auto [path, resolved_msg] = GetOutData();
                if (path.empty()) return;
                std::ofstream out_file(
                    path,
                    std::ios::trunc | (is_json ? std::ios::openmode{} : std::ios::binary));

                if (is_json) {
                    thread_local std::string buffer{};
                    buffer.clear();
                    if (google::protobuf::util::MessageToJsonString(*resolved_msg, &buffer, options).ok()) {
                        out_file << buffer;
                    }
                    else {
                        UEM_ERROR("Failed to write to file: {}", path.string());
                    }
                }
                else if (!resolved_msg->SerializeToOstream(&out_file)) {
                    UEM_ERROR("Failed to write to file: {}", path.string());
                }

                out_file.close();
            };
            std::for_each(std::execution::par_unseq, to_serialize.begin(), to_serialize.end(), GenerateOutFile);

            logger.Stop();
            UEM_INFO("Serialized {} declarations!", to_serialize.size());
        }

    private:
        clang::ASTContext* context{};

        // We use maps to make sure we aren't double visiting, and so we can lookup parent structures when
        // we see that a declaration is nested within another.
        llvm::DenseMap<const clang::Decl*, google::protobuf::Message*> visited_decls{};
        llvm::DenseMap<const clang::Decl*, google::protobuf::Message*> visited_forward_decls{};

        mutable google::protobuf::Arena arena{};
        mutable CountingHeartbeatLogger logger{"Visited {} nodes..."};

        // Equivalent to visited_decls.values() + visited_forward_decls.values() + any TLFileData we make.
        // We use a vector that contains the same data as the maps because we want to parallelize serialization.
        // MSVC vectorizes maps (walks through each node in the map and adds them to a vector) which is an
        // immediate O(N) cost when using parallel std::foreach, while other implementations fall back to non-parallel
        // behavior when a container without random access is used. Rather than dealing with an O(N) cost or silent
        // non-parallization during serialization, we just keep a vector with the same data as we allocate from the
        // arena.
        mutable std::vector<google::protobuf::Message*> to_serialize{};

        // Equivalent to visited_decls.keys() + visited_forward_decls.keys().
        // We keep this vector so that when we generate TLFileData, we can use sorting with random access. This is
        // a more direct tradeoff of performance > memory.
        mutable std::vector<const clang::Decl*> all_unique_visited_decls{};

        struct DeclInfo {
            const char* extension;
            ParseResult::DeclarationMetadata* metadata;
            google::protobuf::Message* message;
        };

        DeclInfo& GetInfo(const clang::Decl* decl) const {
            return GetInfo(visited_decls.contains(decl) ? visited_decls.at(decl) : visited_forward_decls.at(decl));
        }

        static DeclInfo& GetInfo(google::protobuf::Message* msg) {
            static std::unordered_map<google::protobuf::Message*, DeclInfo> cache{};
            if (const auto existing = cache.find(msg); existing != cache.end()) {
                return existing->second;
            }

            if (auto* p_alias = dynamic_cast<ParseResult::TLAliasDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"alias", p_alias->mutable_metadata(), p_alias}}).first->second;
            }
            if (auto* p_enum = dynamic_cast<ParseResult::TLEnumDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"enum", p_enum->mutable_metadata(), p_enum}}).first->second;
            }
            if (auto* p_forward_decl = dynamic_cast<ParseResult::TLForwardDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"fwdecl", p_forward_decl->mutable_metadata(), p_forward_decl}}).first->second;
            }
            if (auto* p_function = dynamic_cast<ParseResult::TLFreeFunctionDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"function", p_function->mutable_metadata(), p_function}}).first->second;
            }
            if (auto* p_variable = dynamic_cast<ParseResult::TLGlobalVariableDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"var", p_variable->mutable_metadata(), p_variable}}).first->second;
            }
            if (auto* p_record = dynamic_cast<ParseResult::TLRecordDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{
                    p_record->kind() == ParseResult::RECORD_KIND_CLASS ? "class"
                    : p_record->kind() == ParseResult::RECORD_KIND_STRUCT ? "struct"
                    : "union",
                    p_record->mutable_metadata(),
                    p_record
                }}).first->second;
            }

            throw std::runtime_error(fmtquill::format("Unsupported declaration message type: {}", msg ? msg->GetTypeName() : "<null>"));
        }

        struct TransientFile {
            std::string_view file_path;
            uint64_t hash;
            uint32_t file_occurrence_index;
            uint32_t local_occurrence_counter;
        };
    };
}
