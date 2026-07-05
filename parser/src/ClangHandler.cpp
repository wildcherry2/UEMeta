// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "UEMeta/ClangHandler.hpp"

#include <atomic>
#include <exception>
#include <execution>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Tooling/Tooling.h>
#include <google/protobuf/util/json_util.h>
#include "parser.pb.h"

#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ASTData.hpp"
#include "UEMeta/Internal/ClangHelpers.hpp"

using namespace ParseResult;

/// @brief Requests traversal of template instantiations.
bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; } // NOLINT(*-convert-member-functions-to-static)

/// @brief Skips implicit compiler-generated declarations.
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; } // NOLINT(*-convert-member-functions-to-static)

/// @brief Skips lambda body traversal.
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; } // NOLINT(*-convert-member-functions-to-static)

/// @brief Runs Clang with ClangHandler and converts guarded exceptions into a nonzero result.
int UEMeta::RunClangTool(clang::tooling::ClangTool& tool) noexcept {
    GClangExceptionCaught.store(false, std::memory_order_relaxed);
    try {
        const auto result = tool.run(clang::tooling::newFrontendActionFactory<ClangHandler>().get());
        return GClangExceptionCaught.load(std::memory_order_relaxed) ? 1 : result;
    } catch (const std::exception& ex) {
        LogClangException("ClangTool::run", ex);
    } catch (...) {
        LogClangUnknownException("ClangTool::run");
    }

    return 1;
}

/// @brief Logs the start of declaration traversal.
void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext& ctx) {
    GuardClangCallback("BeginTranslationUnit", [&] {
        UEM_INFO("Starting AST traversal...");
        data->SetContext(&ctx);
    });
}

/// @brief Logs the end of declaration traversal, starts post-processing
void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) { // NOLINT(*-convert-member-functions-to-static)
    GuardClangCallback("EndTranslationUnit", [] {
        UEM_INFO("Finished traversing AST");
        UEM_INFO("Computing occurrence indices...");
        // now we need to calculate file info/occurrence indices
    });
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;

    auto& context = data->GetContext();
    auto* p_decl = data->Allocate<Declaration>();
    auto* p_record_decl = p_decl->mutable_record();
    PopulateDeclarationMetadata(context, p_record_decl->mutable_metadata(), clang_decl);
    PopulateTemplateDetails(context, p_record_decl->mutable_template_details(), clang_decl);
    p_record_decl->set_is_complete_definition(clang_decl->isCompleteDefinition());
    auto& layout = context.getASTRecordLayout(clang_decl);
    p_record_decl->set_align_bytes(layout.getAlignment().getQuantity());
    p_record_decl->set_size_bytes(layout.getSize().getQuantity());
    p_record_decl->set_kind(clang_decl->isClass() ? RECORD_KIND_CLASS : clang_decl->isStruct() ? RECORD_KIND_STRUCT : RECORD_KIND_UNION);

    const auto ClangToProtoAccess = [&](const clang::AccessSpecifier access) {
        return access == clang::AS_public ? ACCESS_SPECIFIER_PUBLIC
            : access == clang::AS_protected ? ACCESS_SPECIFIER_PROTECTED : access == clang::AS_private ? ACCESS_SPECIFIER_PRIVATE
            : clang_decl->isClass() ? ACCESS_SPECIFIER_PRIVATE : ACCESS_SPECIFIER_PUBLIC;
    };

    // populate instance fields
    for (const auto* field : clang_decl->fields()) {
        auto* p_field = p_record_decl->add_fields();
        const auto type = field->getType();
        p_field->set_access(ClangToProtoAccess(field->getAccess()));
        PopulateIdentifier(context, p_field->mutable_identifier(), field);
        p_field->set_offset_bits(layout.getFieldOffset(field->getFieldIndex()));
        if (field->isBitField()) {
            p_field->set_is_bitfield(true);
            p_field->set_bit_width(field->getBitWidthValue());
        }
        else {
            p_field->set_bit_width(context.getTypeSize(type));
        }
        p_field->set_is_mutable(field->isMutable());
        if (const auto* def_val = field->getInClassInitializer()) {
            p_field->set_default_value(ClangToString(context, def_val));
        }

        static clang::PrintingPolicy field_printing_policy = [&] {
            auto pol = clang::PrintingPolicy(context.getPrintingPolicy());
            pol.SuppressInitializers = true;
            pol.SuppressSpecifiers = false;
            return pol;
        }();

        p_field->set_as_string(ClangToString(field, field_printing_policy));
        p_field->set_content_hash(std::hash<std::string>::operator()(ClangToString(context, field)));
        p_field->set_underlying_type(GetUnderlyingType(field->getType()).getAsString());
        p_field->set_type(field->getType().getAsString());
    }

    // if it's not a c-style POD
    if (const auto* cxx = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(clang_decl)) {
        // populate methods (covers ctors, dtors, conversion ops, static methods, and normal ones)
        for (auto* method : cxx->methods()) {
            if (method->isImplicit()) continue; // we don't care about compiler-generated functions
            auto* p_method = p_record_decl->add_methods();
            PopulateFunctionCommon(context, p_method->mutable_common(), method);

            if (method->isVirtual()) {
                p_method->set_virtuality(method->isPureVirtual() ? FUNCTION_VIRTUALITY_PURE : FUNCTION_VIRTUALITY_VIRTUAL);
                auto* p_vt = p_method->mutable_vtable_index();
                // NOTE: it may be possible to support Linux vtable parsing with Itanium VTableContext when the flags
                // indicate linux compilation, but the math is a bit more complicated
                if (!context.getTargetInfo().getCXXABI().isMicrosoft())
                    throw std::runtime_error("Itanium (Linux) ABI not supported yet!");
                const auto vtable = llvm::cast<clang::MicrosoftVTableContext>(context.getVTableContext());
                const auto method_decl = [&]() -> clang::GlobalDecl {
                    if (const auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method)) {
                        const auto dtor_type = context.getTargetInfo().emitVectorDeletingDtors(
                                                   context.getLangOpts())
                            ? clang::Dtor_VectorDeleting
                            : clang::Dtor_Deleting;
                        return {dtor, dtor_type};
                    }

                    return {method};
                }();
                const auto method_loc = vtable->getMethodVFTableLocation(method_decl);
                p_vt->set_offset(method_loc.VFPtrOffset.getQuantity() / 8);
                p_vt->set_index(method_loc.Index);
            }

            p_method->set_access(ClangToProtoAccess(method->getAccess()));
            p_method->set_is_volatile(method->isVolatile());
            p_method->set_is_const(method->isConst());
            p_method->set_is_deleted(method->isDeleted());
        }

        // populate base classes
        for (const auto& base : cxx->bases()) {
            auto* p_base = p_record_decl->add_bases();
            const auto* base_decl = base.getType()->getAsCXXRecordDecl();
            if (!base_decl) {
                UEM_WARN("Failed to get definition for base type!");
                continue;
            }
            PopulateIdentifier(context, p_base->mutable_identifier(), base_decl->getDefinitionOrSelf());
            p_base->set_access(ClangToProtoAccess(base.getAccessSpecifier()));
            p_base->set_is_virtual(base.isVirtual());
            p_base->set_offset(layout.getBaseClassOffset(base_decl).getQuantity());
            p_base->set_as_string(base.getType().getAsString());
        }
    }

    data->AddVisitedDecl(clang_decl, p_decl);
    return data->OnAfterVisit(clang_decl, p_record_decl->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;

    auto& context = data->GetContext();
    auto* p_decl = data->Allocate<Declaration>();
    auto* p_enum_decl = p_decl->mutable_enum_declaration();
    PopulateDeclarationMetadata(context, p_enum_decl->mutable_metadata(), clang_decl);
    PopulateEnumDetails(context, p_enum_decl->mutable_details(), clang_decl);
    for (auto* enumerator : clang_decl->enumerators()) {
        auto p_enumerator = p_enum_decl->add_enumerators();
        PopulateIdentifier(context, p_enumerator->mutable_identifier(), enumerator);
        p_enumerator->set_value(llvm::toString(enumerator->getInitVal(), 10));
    }

    data->AddVisitedDecl(clang_decl, p_decl);
    return data->OnAfterVisit(clang_decl, p_enum_decl->mutable_metadata()->identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl* clang_decl) {
    if (clang_decl->isCXXClassMember() || data->OnVisit(clang_decl)) return true; // handled by VisitRecordDecl

    auto& context = data->GetContext();
    auto* p_decl = data->Allocate<Declaration>();
    auto* p_fun = p_decl->mutable_function();
    PopulateDeclarationMetadata(context, p_fun->mutable_metadata(), clang_decl);
    PopulateFunctionCommon(context, p_fun->mutable_common(), clang_decl);
    data->AddVisitedDecl(clang_decl, p_decl);
    return data->OnAfterVisit(clang_decl, p_fun->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;
    auto& context = data->GetContext();
    auto* p_decl = data->Allocate<Declaration>();
    auto* p_alias = p_decl->mutable_alias();
    PopulateDeclarationMetadata(context, p_alias->mutable_metadata(), clang_decl);
    PopulateTemplateDetails(context, p_alias->mutable_template_details(), clang_decl);
    p_alias->set_alias(clang_decl->getNameAsString());
    p_alias->set_aliased_type(clang_decl->getUnderlyingType().getAsString());
    const auto str = ClangToString(context, clang_decl);
    p_alias->set_as_string(str);
    data->AddVisitedDecl(clang_decl, p_decl);
    return true;
}

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl* clang_decl) {
    if (clang_decl->isCXXClassMember() || clang::dyn_cast_or_null<clang::ParmVarDecl>(clang_decl) // handled by VisitRecordDecl/VisitFunctionDecl
        || data->OnVisit(clang_decl))
        return true;
    auto& context = data->GetContext();
    auto* p_decl = data->Allocate<Declaration>();
    auto* p_var = p_decl->mutable_variable();
    PopulateDeclarationMetadata(context, p_var->mutable_metadata(), clang_decl);
    p_var->set_underlying_type(GetUnderlyingType(clang_decl->getType()).getAsString());
    p_var->set_type(clang_decl->getType().getAsString());
    const auto str = ClangToString(context, clang_decl);
    p_var->set_as_string(str);
    p_var->set_content_hash(std::hash<std::string>::operator()(str));
    p_var->set_storage_class(clang_decl->getTLSKind() != clang::VarDecl::TLS_None ? VAR_STORAGE_CLASS_THREAD_LOCAL
        : clang_decl->getStorageClass() == clang::SC_Static ? VAR_STORAGE_CLASS_STATIC
        : clang_decl->getStorageClass() == clang::SC_Extern && clang_decl->isExternC() ? VAR_STORAGE_CLASS_EXTERN_C
        : clang_decl->getStorageClass() == clang::SC_Extern && clang_decl->isInExternCXXContext() ? VAR_STORAGE_CLASS_EXTERN
        : VAR_STORAGE_CLASS_UNSPECIFIED);
    p_var->set_constant_evaluation_kind(clang_decl->isConstexpr() ? CONSTANT_EVALUATION_CONSTEXPR : CONSTANT_EVALUATION_NONE); // vars can't be consteval
    p_var->set_default_value(ClangToString(context, clang_decl->getInit()));
    data->AddVisitedDecl(clang_decl, p_decl);
    return data->OnAfterVisit(clang_decl, p_var->metadata().identifier().qualified_name_hash());
}

UEMeta::ClangHandler::ClangHandler() : data(new ASTData()) {}

/// @brief Creates the AST consumer for one translation unit.
std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    try {
        compiler.getLangOpts().CommentOpts.ParseAllComments = true;
        const auto input_name = file.str();
        const auto file_name = std::filesystem::path(input_name).filename().string();
        const auto tu_name = file_name.empty() ? input_name : file_name;
        UEM_INFO("Parsing TU '{}' (this may take a moment)", tu_name);

        /// @brief AST consumer that drives traversal once Clang finishes parsing a translation unit.
        class Consumer : public clang::ASTConsumer {
        public:
            /// @brief Creates a consumer tied to the owning ClangHandler.
            explicit Consumer(ClangHandler* owner, std::string tu_name)
                : owner(owner), tu_name(std::move(tu_name)) {}

            /// @brief Traverses declarations after Clang finishes parsing the translation unit.
            void HandleTranslationUnit(clang::ASTContext& ctx) override {
                GuardClangCallback("HandleTranslationUnit", [&] {
                    UEM_INFO("TU '{}' parsed!", tu_name);
                    owner->BeginTranslationUnit(ctx);
                    if (!owner->TraverseDecl(ctx.getTranslationUnitDecl())) {
                        UEM_ERROR("(clang) AST traversal aborted due to an earlier exception.");
                        return;
                    }
                    owner->Serialize();
                    owner->EndTranslationUnit(ctx);
                });
            }

        private:
            ClangHandler* owner;
            std::string tu_name;
        };

        return std::make_unique<Consumer>(this, tu_name);
    } catch (const std::exception& ex) {
        LogClangException("CreateASTConsumer", ex);
        throw;
    } catch (...) {
        LogClangUnknownException("CreateASTConsumer");
        throw;
    }
}

void UEMeta::ClangHandler::Serialize() const {
    const auto& cfg = Config::GetConfig();
    const auto& messages = data->GetAllMessages();
    if (cfg.DumpToJson()) {
        std::string json = "[\n";
        std::string buffer{};
        constexpr google::protobuf::json::PrintOptions options {.add_whitespace = true, .always_print_fields_with_no_presence = true };
        for (const auto* msg : messages) {
            if (google::protobuf::util::MessageToJsonString(*msg, &buffer, options).ok()) {
                json += buffer + ",\n";
            }
            buffer.clear();
        }
        if (!messages.empty()) {
            json.pop_back();
            json.pop_back();
        }
        json += "\n]";
        const auto& log_path = Config::GetConfig().Log().UnderlyingPath();
        const auto out_path = (log_path.empty() ? StablePath::current_program_directory().UnderlyingPath() : log_path) / "dump.json";
        std::ofstream json_file(out_path, std::ios::trunc);
        json_file << json;
        json_file.close();
        return;
    }

    const auto out_dir = cfg.OutputDirectory().UnderlyingPath();
    const auto is_json = cfg.Format() == Config::SerializationFormat::json;

    // Actually generates the file
    const auto GenerateOutFile = [&](const google::protobuf::Message* msg) {
        if (!msg) return;

        // validate if in debug
        #if defined(DEBUG)
        auto validator = data->CreateValidator();
        auto results = validator.Validate(*msg);
        if (!results.ok()) {
            UEM_ERROR("Validation error encountered: {}", results.status().message());
            return;
        }
        if (const auto& validation_result = results.value(); !validation_result.success()) {
            for (const auto& err : validation_result.violations()) {
                UEM_ERROR("Proto validation failed: {}", err.proto().ShortDebugString());
            }
            return;
        }
        #endif

        const auto* decl = dynamic_cast<const Declaration*>(msg);
        if (!decl) {
            UEM_WARN("Skipping unsupported serialization message: {}", msg->GetTypeName());
            return;
        }

        // file path is in format {qualnamehash}-{contenthash}-{filenamehash}-{occurrenceindex}.[class|struct|enum|union|alias|function|fwdecl|var][bin|json]
        auto out_path = out_dir;
        using p = std::tuple<const char*, const DeclarationMetadata&, const google::protobuf::Message&>;
        auto [extension, meta, out_msg] = decl->has_alias() ? p{"alias", decl->alias().metadata(), decl->alias()}
            : decl->has_enum_declaration() ? p{"enum", decl->enum_declaration().metadata(), decl->enum_declaration()}
            : decl->has_forward_declaration() ? p{"fwdecl", decl->forward_declaration().metadata(), decl->forward_declaration()}
            : decl->has_function() ? p{"function", decl->function().metadata(), decl->function()}
            : decl->has_variable() ? p{"var", decl->variable().metadata(), decl->variable()}
            : decl->record().kind() == RECORD_KIND_CLASS ? p{"class", decl->record().metadata(), decl->record()}
            : decl->record().kind() == RECORD_KIND_STRUCT ? p{"struct", decl->record().metadata(), decl->record()}
            : p{"union", decl->record().metadata(), decl->record()};

        const auto& ident = meta.identifier();
        out_path = out_path / fmtquill::format("{}-{}-{}-{}.{}{}", ident.qualified_name_hash(),
            meta.content_hash(), ident.file_path_hash(), meta.occurrence_index(), extension, is_json ? "json" : "bin");
        std::ofstream out_file(out_path, std::ios::trunc);

        if (is_json) {
            thread_local std::string buffer{};
            constexpr google::protobuf::json::PrintOptions options {.add_whitespace = true, .always_print_fields_with_no_presence = true };
            buffer.clear();
            if (google::protobuf::util::MessageToJsonString(out_msg, &buffer, options).ok()) {
                out_file << buffer;
            }
            else {
                UEM_ERROR("Failed to write to file: {}", out_path.string());
            }
        }
        else if (!out_msg.SerializeToOstream(&out_file)) {
            UEM_ERROR("Failed to write to file: {}", out_path.string());
        }

        out_file.close();
    };

    std::for_each(std::execution::par_unseq, messages.begin(), messages.end(), GenerateOutFile);
}
