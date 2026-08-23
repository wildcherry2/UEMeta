#pragma once
#include <algorithm>
#include <execution>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include "TopLevel.pb.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "llvm/ADT/DenseMap.h"
#include "clang/AST/Decl.h"
#include "../Cli.hpp"
#include "ClangHelpers.hpp"

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

        template<typename DeclType>
        [[nodiscard]] bool OnVisit(const DeclType* decl) {
            // don't process a nullptr
            if (!decl) return true;

            // if the decl is within the local scope of a function or method, don't process it
            if constexpr (std::same_as<DeclType, clang::FunctionDecl>
                            || std::same_as<DeclType, clang::RecordDecl>
                            || std::same_as<DeclType, clang::EnumDecl>
                            || std::same_as<DeclType, clang::TypedefNameDecl>) {
                if (decl->getDeclContext()->isFunctionOrMethod()) {
                    return true;
                }
            }
            else if constexpr(std::same_as<DeclType, clang::VarDecl>) {
                if (decl->isLocalVarDecl() // don't care about local variables
                    || decl->isCXXClassMember() // fields are handled in record visitation
                    || clang::dyn_cast<clang::ParmVarDecl>(decl)) { // params are handled in function visitation
                    return true;
                }
            }

            // member functions are handled in record visitation
            if constexpr(std::same_as<DeclType, clang::FunctionDecl>) {
                if (decl->isCXXClassMember()) {
                    return true;
                }
            }

            // if the decl is in a builtin path, make sure we know about it, but don't process it
            if (IsDeclFromBuiltinFile(*context, decl)) {
                builtin_paths.insert(NormalizePath(GetDeclIncludePath(*context, decl)));
                return true;
            }

            // if the user doesn't want implicit specializations, don't process it if it's implicit
            if (!Config::GetConfig().ProcessImplicitSpecializations()) {
                if constexpr (std::same_as<DeclType, clang::FunctionDecl>
                                || std::same_as<DeclType, clang::VarDecl>) {
                    if (decl->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation) {
                        return true;
                    }
                }
                else if constexpr(std::same_as<DeclType, clang::RecordDecl>) {
                    if (decl->getDeclContext()->isFunctionOrMethod()) return true;
                    if (const clang::CXXRecordDecl* as_cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
                        if (as_cxx->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation) {
                            return true;
                        }
                    }
                }
            }

            if (const auto as_tag = llvm::dyn_cast_or_null<clang::TagDecl>(decl)) {
                // if this is a forward declaration...
                if (!as_tag->isThisDeclarationADefinition()) {
                    // and we don't know about it yet...
                    if (!visited_forward_decls.contains(decl)) {
                        // generate a new forward declaration message and return
                        auto& ast_context = GetContext();
                        auto* p_forward_decl = Allocate<ParserTypes::TLForwardDeclaration>();
                        PopulateDeclarationMetadata(ast_context, p_forward_decl->mutable_metadata(), decl);

                        // when the forward declaration is templated, we need to explicitly get the template decl
                        const clang::Decl* printable_decl = decl;
                        if (const auto* record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
                            if (const auto* template_decl = record_decl->getDescribedClassTemplate()) {
                                printable_decl = template_decl;
                            }
                        }
                        Proto::SetVersioned(
                            p_forward_decl->mutable_as_string(), ClangToString(ast_context, printable_decl));

                        // assign the kind and template info
                        switch (as_tag->getTagKind()) {
                            case clang::TagTypeKind::Struct:
                                p_forward_decl->set_kind(ParserTypes::FORWARD_DECLARATION_KIND_STRUCT);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Union:
                                p_forward_decl->set_kind(ParserTypes::FORWARD_DECLARATION_KIND_UNION);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Class:
                                p_forward_decl->set_kind(ParserTypes::FORWARD_DECLARATION_KIND_CLASS);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Enum:
                                p_forward_decl->set_kind(ParserTypes::FORWARD_DECLARATION_KIND_ENUM);
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
                    if (auto* parent_record = dynamic_cast<ParserTypes::TLRecordDeclaration*>(parent->getSecond())) {
                        Proto::AddVersioned(parent_record->mutable_nested_hashes(), clang_decl_fqn_hash);
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
                // Declarations produced by a macro can share an expansion location. Keep
                // their visitation order so occurrence indices remain deterministic.
                std::stable_sort(
                    std::execution::seq,
                    all_unique_visited_decls.begin(),
                    all_unique_visited_decls.end(),
                    [&](const clang::Decl* lhs, const clang::Decl* rhs) {
                        return sm.isBeforeInTranslationUnit(
                            sm.getExpansionLoc(lhs->getLocation()),
                            sm.getExpansionLoc(rhs->getLocation()));
                    });
                sort_logger.Stop();
                UEM_INFO("Sorted {} declarations!", all_unique_visited_decls.size());
            }

            // assign occurrence indices and generate TLFileData
            {
                CountingHeartbeatLogger occ_logger{"Generating TLFileData from declarations ({} declarations remaining)..."};
                occ_logger.SetValue(all_unique_visited_decls.size());
                occ_logger.Start();

                const auto AddBuiltinIncludes = [this](
                    TLFileData* file_data,
                    const std::set<std::string>& source_paths) {
                    std::set<std::string> file_includes{};
                    for (const auto& source_path : source_paths) {
                        const auto [first, last] = includes.equal_range(source_path);
                        for (auto include = first; include != last; ++include) {
                            file_includes.insert(include->second);
                        }
                    }
                    if (file_includes.empty()) {
                        UEM_INFO("No includes for file {}!", file_data->path());
                    }

                    std::vector<std::string> builtin_includes(
                        std::min(file_includes.size(), builtin_paths.size()));
                    const auto intersection_end = std::set_intersection(
                        std::execution::par_unseq,
                        file_includes.begin(),
                        file_includes.end(),
                        builtin_paths.begin(),
                        builtin_paths.end(),
                        builtin_includes.begin());

                    for (const auto& include : std::ranges::subrange(builtin_includes.begin(), intersection_end)) {
                        Proto::AddVersioned(file_data->mutable_builtin_includes(), include);
                    }
                };

                const auto InitializeFileData = [](TLFileData* file_data) {
                    Proto::MutableVersionItem(file_data->mutable_defined_type_hashes());
                    Proto::MutableVersionItem(file_data->mutable_forward_declaration_hashes());
                    Proto::MutableVersionItem(file_data->mutable_builtin_includes());
                    return file_data;
                };

                auto* current_file = InitializeFileData(Allocate<ParserTypes::TLFileData>());
                current_file->set_path(
                    Proto::GetVersioned(GetInfo(all_unique_visited_decls[0]).metadata->identifier().file_path()));
                Proto::SetVersioned(current_file->mutable_file_occurrence(), 0);
                current_file->set_path_hash(
                    Proto::GetVersioned(GetInfo(all_unique_visited_decls[0]).metadata->identifier().file_path_hash()));
                std::set<std::string> current_file_source_paths{
                    NormalizePath(GetDeclIncludePath(*context, all_unique_visited_decls[0]))};

                uint32_t occurrence_counter = 0;
                const auto AddDeclarationToFile = [&occurrence_counter](TLFileData* file_data, const DeclInfo& info) {
                    Proto::SetVersioned(info.metadata->mutable_occurrence_index(), occurrence_counter++);
                    if (info.extension != "fwdecl") {
                        Proto::AddVersioned(
                            file_data->mutable_defined_type_hashes(),
                            info.metadata->identifier().qualified_name_hash());
                    }
                    else {
                        Proto::AddVersioned(
                            file_data->mutable_forward_declaration_hashes(),
                            info.metadata->identifier().qualified_name_hash());
                    }
                };

                for (auto decl_it = all_unique_visited_decls.begin(); decl_it != all_unique_visited_decls.end(); ++decl_it) {
                    auto* info = &GetInfo(*decl_it);

                    // switching to a new file, now we need to make sure there are no gaps and patch if there are
                    if (const auto file_hash = Proto::GetVersioned(info->metadata->identifier().file_path_hash());
                        file_hash != current_file->path_hash()) {
                        auto subrange = std::ranges::find_last_if(std::next(decl_it), all_unique_visited_decls.end(),
                    [current_file, this](const clang::Decl* decl) {
                            return Proto::GetVersioned(GetInfo(decl).metadata->identifier().file_path_hash())
                                == current_file->path_hash();
                        });

                        // found a gap, patch it while processing it
                        if (!subrange.empty()) {
                            for (auto end = subrange.begin(); decl_it != end; ++decl_it) {
                                const auto& to_patch_info = GetInfo(*decl_it);
                                current_file_source_paths.insert(
                                    NormalizePath(GetDeclIncludePath(*context, *decl_it)));
                                Proto::SetVersioned(
                                    to_patch_info.metadata->mutable_identifier()->mutable_file_path(),
                                    current_file->path());
                                Proto::SetVersioned(
                                    to_patch_info.metadata->mutable_identifier()->mutable_file_path_hash(),
                                    current_file->path_hash());
                                AddDeclarationToFile(current_file, to_patch_info);
                                occ_logger.Decrement();
                            }
                            info = &GetInfo(*decl_it);
                            // fall through to fill out occurrence index for current decl_it
                        }

                        // no gaps, update file stuff and reset occurrence counter
                        else {
                            AddBuiltinIncludes(current_file, current_file_source_paths);
                            const auto occ = Proto::GetVersioned(current_file->file_occurrence()) + 1;
                            current_file = InitializeFileData(Allocate<TLFileData>());
                            current_file->set_path(
                                Proto::GetVersioned(info->metadata->identifier().file_path()));
                            Proto::SetVersioned(current_file->mutable_file_occurrence(), occ);
                            current_file->set_path_hash(
                                Proto::GetVersioned(info->metadata->identifier().file_path_hash()));
                            current_file_source_paths = {
                                NormalizePath(GetDeclIncludePath(*context, *decl_it))};
                            occurrence_counter = 0;
                        }
                    }

                    AddDeclarationToFile(current_file, *info);
                    occ_logger.Decrement();
                }

                AddBuiltinIncludes(current_file, current_file_source_paths);

                occ_logger.Stop();
                UEM_INFO(
                    "Generated {} TLFileData!",
                    current_file ? Proto::GetVersioned(current_file->file_occurrence()) + 1 : 0);
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
                const auto wrapper = google::protobuf::Arena::Create<ParserTypes::TLItemList>(&arena);
                const auto list = wrapper->mutable_items();
                for (auto* msg : to_serialize) {
                    auto item = google::protobuf::Arena::Create<TLItem>(&arena);
                    dynamic_cast<TLRecordDeclaration*>(msg) ? item->set_allocated_record(static_cast<TLRecordDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLEnumDeclaration*>(msg) ? item->set_allocated_enum_declaration(static_cast<TLEnumDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLForwardDeclaration*>(msg) ? item->set_allocated_forward_declaration(static_cast<TLForwardDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLAliasDeclaration*>(msg) ? item->set_allocated_alias(static_cast<TLAliasDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLFreeFunctionDeclaration*>(msg) ? item->set_allocated_function(static_cast<TLFreeFunctionDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLGlobalVariableDeclaration*>(msg) ? item->set_allocated_variable(static_cast<TLGlobalVariableDeclaration*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
                        : dynamic_cast<TLFileData*>(msg) ? item->set_allocated_file_data(static_cast<TLFileData*>(msg)) // NOLINT(*-pro-type-static-cast-downcast)
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
                    if (const auto* p_file = dynamic_cast<ParserTypes::TLFileData*>(msg)) {
                        if (cfg.PrefersFullNameInFileName()) {
                            return {out_dir / fmtquill::format("{}-{}.file{}",
                                std::filesystem::path{p_file->path()}.filename().string(),
                                Proto::GetVersioned(p_file->file_occurrence()), is_json ? "json" : "bin"), msg};
                        }
                        // file path is in format {filepathhash}-{fileoccurrenceindex}.file[bin|json]
                        return {out_dir / fmtquill::format("{}-{}.file{}",
                            p_file->path_hash(), Proto::GetVersioned(p_file->file_occurrence()),
                            is_json ? "json" : "bin"), msg};
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

                    if (cfg.PrefersFullNameInFileName()) {
                        return {out_dir / fmtquill::format("{}-{}-{}-{}.{}{}", ident.qualified_name(),
                        Proto::GetVersioned(info->metadata->content_hash()),
                        Proto::GetVersioned(ident.file_path_hash()),
                        Proto::GetVersioned(info->metadata->occurrence_index()),
                        info->extension, is_json ? "json" : "bin"), info->message};
                    }

                    return {out_dir / fmtquill::format("{}-{}-{}-{}.{}{}", ident.qualified_name_hash(),
                        Proto::GetVersioned(info->metadata->content_hash()),
                        Proto::GetVersioned(ident.file_path_hash()),
                        Proto::GetVersioned(info->metadata->occurrence_index()),
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

        void Include(const std::string& source, const std::string& include) {
            includes.emplace(NormalizePath(source), NormalizePath(include));
        }

        size_t GetIncludeCount() const {
            return includes.size();
        }

    private:
        static std::string NormalizePath(const std::string_view path) {
            return std::filesystem::path{std::string{path}}.lexically_normal().generic_string();
        }

        static std::string GetDeclIncludePath(clang::ASTContext& context, const clang::Decl* decl) {
            const auto& source_manager = context.getSourceManager();
            const auto source_location = source_manager.getExpansionLoc(decl->getLocation());
            if (const auto* file = source_manager.getFileEntryForID(source_manager.getFileID(source_location))) {
                const auto real_path = file->tryGetRealPathName();
                if (!real_path.empty()) return real_path.str();
            }
            return source_manager.getFilename(source_location).str();
        }

        clang::ASTContext* context{};

        // We use maps to make sure we aren't double visiting, and so we can lookup parent structures when
        // we see that a declaration is nested within another.
        llvm::DenseMap<const clang::Decl*, google::protobuf::Message*> visited_decls{};
        llvm::DenseMap<const clang::Decl*, google::protobuf::Message*> visited_forward_decls{};
        std::set<std::string> builtin_paths{};

        // maps file -> include paths
        std::unordered_multimap<std::string, std::string> includes{};

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
            std::string_view extension;
            ParserTypes::DeclarationMetadata* metadata;
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

            if (auto* p_alias = dynamic_cast<ParserTypes::TLAliasDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"alias", p_alias->mutable_metadata(), p_alias}}).first->second;
            }
            if (auto* p_enum = dynamic_cast<ParserTypes::TLEnumDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"enum", p_enum->mutable_metadata(), p_enum}}).first->second;
            }
            if (auto* p_forward_decl = dynamic_cast<ParserTypes::TLForwardDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"fwdecl", p_forward_decl->mutable_metadata(), p_forward_decl}}).first->second;
            }
            if (auto* p_function = dynamic_cast<ParserTypes::TLFreeFunctionDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"function", p_function->mutable_metadata(), p_function}}).first->second;
            }
            if (auto* p_variable = dynamic_cast<ParserTypes::TLGlobalVariableDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{"var", p_variable->mutable_metadata(), p_variable}}).first->second;
            }
            if (auto* p_record = dynamic_cast<ParserTypes::TLRecordDeclaration*>(msg)) {
                return cache.insert(std::pair{msg, DeclInfo{
                    p_record->kind() == ParserTypes::RECORD_KIND_CLASS ? "class"
                    : p_record->kind() == ParserTypes::RECORD_KIND_STRUCT ? "struct"
                    : "union",
                    p_record->mutable_metadata(),
                    p_record
                }}).first->second;
            }

            throw std::runtime_error(fmtquill::format("Unsupported declaration message type: {}", msg ? msg->GetTypeName() : "<null>"));
        }
    };
}
