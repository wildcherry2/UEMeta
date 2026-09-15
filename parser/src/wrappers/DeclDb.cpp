#include "UEMeta/wrappers/DeclDb.hpp"

#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "UEMeta/wrappers/Utility.hpp"
#include "UEMeta/wrappers/RecordDeclWrapper.hpp"
#include "UEMeta/wrappers/VarDeclWrapper.hpp"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "CLI/ConfigFwd.hpp"
#include "google/protobuf/util/json_util.h"

BS::thread_pool<> UEMeta::DeclDb::serialization_pool;

static bool isDeclInFunctionOrMethod(const clang::Decl* decl);
static bool isDeclInSystemOrStdHeader(const clang::Decl* decl);

template<UEMeta::TagDeclDerived T>
static bool isTagAnonymousAndEmbedded(const T* decl) {
    if (!decl) return false;
    return decl->getName().empty() && decl->getTypedefNameForAnonDecl() == nullptr && decl->isEmbeddedInDeclarator() && !decl->isFreeStanding();
}

template<UEMeta::TemplateSpecializableDeclType T>
static bool isImplicitSpec(const T* decl) {
    if (!decl) return false;
    return decl->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation;
}

template<UEMeta::TemplateSpecializableDeclType T>
static bool failsImplicitSpecOption(const T* decl) {
    if (!decl) return false;
    if (UEMeta::Config::GetConfig().ProcessImplicitSpecializations()) return true;
    return isImplicitSpec(decl);
}

void UEMeta::DeclDb::addDeclIdentity(clang::Decl* decl, const Hash& hash) {
    if (!decl) throw std::runtime_error("Can't addDeclIdentity with null Decl pointer!");
    decl_to_identity_map.insert({decl, hash});
    identity_to_decl_map.insert({hash, decl});
    visited_decls.insert(decl);
}

UEMeta::DeclDb::QueryResult UEMeta::DeclDb::queryDeclIdentity(const clang::Decl* decl) {
    try {
        if (!decl) return false;
        if (const auto decl_hash_it = decl_to_identity_map.find(decl); decl_hash_it != decl_to_identity_map.end()) {
            return decl_hash_it->second;
        }

        const auto& sm = decl->getASTContext().getSourceManager();
        if (const auto loc = sm.getSpellingLoc(decl->getLocation()); decl->isInStdNamespace() || sm.isInSystemHeader(loc) || sm.isInSystemMacro(loc)) {
            auto file = sm.getFileEntryRefForID(sm.getFileID(loc));
            if (!file) return false;
            return llvm::sys::path::filename(file->getName());
        }

        if (const auto fwd_hash_it = decl_to_forward_decl_occurrence_map.find(decl); fwd_hash_it != decl_to_forward_decl_occurrence_map.end()) {
            std::variant<uint64_t, clang::Decl*>& last = fwd_hash_it->second.back();
            // this should theoretically always be the case, since if it's a Decl, it's in the decl_to_identity_map and this never happens
            if (uint64_t* as_fwd = std::get_if<uint64_t>(&last)) {
                return *as_fwd;
            }
        }

        return false;
    } catch (...) {
        return std::monostate{};
    }
}

clang::Decl* UEMeta::DeclDb::queryDecl(const Hash& hash) {
    if (!hash) return nullptr;
    const auto decl_it = identity_to_decl_map.find(hash);
    if (decl_it == identity_to_decl_map.end()) return nullptr;
    return decl_it->second;
}

UEMeta::DeclDb::QueryResult UEMeta::DeclDb::queryType(clang::QualType type, clang::QualType* unwrapped_type) {
    if (unwrapped_type) *unwrapped_type = {};
    try {
        // Resolve aliases, then strip structural layers that have no declaration identity.
        // Callers keep the original QualType for spelling; only this lookup peels Node*[N] to Node.
        if (type.isNull()) return false;
        type = type.getCanonicalType();
        while (true) {
            if (type->isPointerType() || type->isReferenceType()) {
                type = type->getPointeeType();
            }
            else if (const auto* array = llvm::dyn_cast<clang::ArrayType>(type.getTypePtr())) {
                type = array->getElementType();
            }
            else break;
        }
        if (unwrapped_type) *unwrapped_type = type;

        // Select the source declaration BEFORE querying: a generated instantiation is never
        // a reference target, even if it happens to have a registered hash. Clang's tag
        // conversion already prefers the definition, matching the keys used by DeclDb.
        if (const auto* target = type->getAsTagDecl()) {
            if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(target)) {
                // The pattern is the primary record or selected partial specialization, not
                // necessarily the primary. Written primary/partial/explicit specializations
                // have no instantiation pattern and retain their own declaration identity.
                // Explicit instantiations also refer to their pattern; they are not explicit specializations.
                if (const auto* pattern = record->getTemplateInstantiationPattern()) {
                    target = pattern->getDefinitionOrSelf();
                }
                else if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
                         specialization && !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(specialization)
                         && specialization->getSpecializationKind() != clang::TSK_ExplicitSpecialization) {
                    // A use such as Box<int>* can exist before Clang selects an instantiation
                    // pattern. Do not guess a primary/partial or query the generated placeholder.
                    return false;
                }
            }
            else if (const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(target)) {
                // Enums instantiated as members of class templates likewise refer to the source enum.
                if (const auto* pattern = enumeration->getTemplateInstantiationPattern()) {
                    target = pattern->getDefinitionOrSelf();
                }
            }

            // Preserve the selected declaration's hash, forward occurrence, header or error.
            // An unknown selected pattern must not fall back to its generated instantiation.
            const QueryResult result = queryDeclIdentity(target);
            if (const bool* known = std::get_if<bool>(&result); !known || *known) return result;
        }

        // Classify types without a known declaration only after identity lookup, so dependent
        // record identities are not hidden by the generic builtin/template marker.
        return type->isBuiltinType() || type->isDependentType() || type->isTemplateTypeParmType();
    } catch (...) {
        return std::monostate{};
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::EnumDecl* decl) {
    try {
        if (!decl) return;
        if (visited_decls.contains(decl)) return;
        visited_decls.insert(decl);
        if (isDeclInSystemOrStdHeader(decl)) return;
        // if this enum is completely anonymous and attached to a declarator (like a VarDecl or FieldDecl), we'll get it later
        if (isTagAnonymousAndEmbedded(decl)) return;
        if (isDeclInFunctionOrMethod(decl)) return;

        if (!decl->isThisDeclarationADefinition()) {
            auto* def = decl->getDefinition();
            if (!def) {
                //todo log or something since this is an undefined enum
                return;
            }
            addForwardDeclaration(def);
            return;
        }

        const auto arena = std::make_shared<google::protobuf::Arena>();
        const auto result = EnumDeclWrapper(decl, arena).serialize();
        if (const auto* variables = std::get_if<std::vector<ParserTypes::TLGlobalVariableDeclaration*>>(&result)) {
            for (const auto* variable : *variables) {
                serialize(variable, arena);
            }
        }
        else if (const auto* p_enum = std::get_if<ParserTypes::TLEnumDeclaration*>(&result)) {
            serialize(*p_enum, arena);
        }
    } catch (std::exception& e) {
        UEM_ERROR("{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::VarDecl *decl) {
    try {
        if (!decl) return;
        if (visited_decls.contains(decl)) return;
        visited_decls.insert(decl);
        if (isDeclInSystemOrStdHeader(decl)) return;
        if (isDeclInSystemOrStdHeader(decl->getTemplateInstantiationPattern())) return;
        if (decl->isLocalVarDeclOrParm()) return;
        if (decl->isCXXClassMember()) return;
        if (failsImplicitSpecOption(decl)) return;
        if (isDeclInFunctionOrMethod(decl)) return;

        const auto arena = std::make_shared<google::protobuf::Arena>();
        const auto result = VarDeclWrapper(decl, arena).serialize();
        serialize(result, arena);
    } catch (std::exception& e) {
        UEM_ERROR("{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::RecordDecl* decl) {
    try {
        // Records already consumed by an enclosing wrapper must not be serialized again by the visitor.
        if (!decl || visited_decls.contains(decl)) return;
        visited_decls.insert(decl);
        if (decl->isImplicit() || isDeclInSystemOrStdHeader(decl) || isDeclInFunctionOrMethod(decl)) return;
        if (isTagAnonymousAndEmbedded(decl)) return;

        // Honor instantiation filtering while preserving explicit and partial specializations.
        // todo remove and deny by default when implicit option is taken out
        if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
            if (isDeclInSystemOrStdHeader(cxx->getTemplateInstantiationPattern())) return;
            if (!Config::GetConfig().ProcessImplicitSpecializations()
                && cxx->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation) return;
        }

        // Semantically anonymous member storage is extracted by its nearest owning record.
        if (decl->isAnonymousStructOrUnion()
            && decl->getDeclContext()->getNonTransparentContext()->isRecord()) return;

        // Record the forward occurrence without consuming the eventual definition's visit.
        if (!decl->isThisDeclarationADefinition()) {
            if (auto* definition = decl->getDefinition()) {
                addForwardDeclaration(definition);
            }
            return;
        }

        const auto arena = std::make_shared<google::protobuf::Arena>();
        for (const auto results = RecordDeclWrapper(decl, arena).serialize(); const auto& record_result : results) {
            if (auto* p_record = std::get_if<ParserTypes::TLRecordDeclaration*>(&record_result)) {
                serialize(*p_record, arena);
            }
            else if (const auto* variables = std::get_if<std::vector<ParserTypes::TLGlobalVariableDeclaration*>>(&record_result)) {
                for (const auto* variable : *variables) {
                    serialize(variable, arena);
                }
            }
            else if (auto* p_enum = std::get_if<ParserTypes::TLEnumDeclaration*>(&record_result)) {
                serialize(*p_enum, arena);
            }
        }
    }
    catch (const std::exception& e) {
        UEM_ERROR("{}", e.what());
    }
}

void UEMeta::DeclDb::addForwardDeclaration(clang::TagDecl* forDecl) {
    if (!forDecl || !forDecl->isThisDeclarationADefinition()) {
        throw std::invalid_argument("Failed to addForwardDeclaration because the declaration is not a definition!");
    }
    if (const auto other_decls = decl_to_forward_decl_occurrence_map.find(forDecl); other_decls != decl_to_forward_decl_occurrence_map.end()) {
        other_decls->second.emplace_back(allocateDeclOccurrence());
    }
    else {
        decl_to_forward_decl_occurrence_map.insert({forDecl, {std::variant<uint64_t, clang::Decl*>{allocateDeclOccurrence()}}});
    }
}

void UEMeta::DeclDb::addDeclarationAsVisited(clang::Decl* decl) {
    if (!decl) {
        throw std::invalid_argument("Failed to addDeclarationAsVisited because decl is null!");
    }

    visited_decls.insert(decl);
}

void UEMeta::DeclDb::awaitPendingSerializations() {
    serialization_pool.wait();
}

bool isDeclInFunctionOrMethod(const clang::Decl* decl) {
    if (!decl) return false;

    auto recIsInFunctionOrMethod = [](this auto self, const clang::DeclContext* decl_context) {
        if (!decl_context) return false;
        if (decl_context->isFunctionOrMethod()) return true;
        return self(decl_context->getParent());
    };

    return recIsInFunctionOrMethod(decl->getDeclContext());
}

bool isDeclInSystemOrStdHeader(const clang::Decl *decl) {
    if (!decl) return false;
    if (decl->isInStdNamespace()) return true;

    const auto& sm = decl->getASTContext().getSourceManager();
    const auto loc = sm.getSpellingLoc(decl->getLocation());
    return sm.isInSystemHeader(loc) || sm.isInSystemMacro(loc);
}

template<UEMeta::TopLevelDecl T>
static std::filesystem::path getSerializationPath(const T* msg) {
    thread_local const auto& cfg = UEMeta::Config::GetConfig();
    thread_local const auto& out_dir = cfg.OutputDirectory().UnderlyingPath();
    thread_local const auto is_json = cfg.Format() == UEMeta::Config::SerializationFormat::json;
    thread_local const std::string_view type = is_json ? "json" : "bin";

    std::filesystem::path out_file_path;
    const ParserTypes::DeclarationMetadata& metadata = msg->metadata();
    if (cfg.PrefersFullNameInFileName()) {
        auto name = std::string_view{metadata.qualified_name()}
            | std::views::split(std::string_view{"::"})
            | std::views::join_with(std::string_view{"::"})
            | std::ranges::to<std::string>();
        out_file_path = out_dir / fmtquill::format("{}-{}.{}{}",
            name,
            metadata.occurrence_index().versions(0).value(),
            UEMeta::TOP_LEVEL_EXT<T>,
            type);
    }
    else {
        out_file_path = out_dir / fmtquill::format("{}{}-{}.{}{}",
            metadata.decl_id().a(), metadata.decl_id().b(),
            metadata.occurrence_index().versions(0).value(),
            UEMeta::TOP_LEVEL_EXT<T>,
            type);
    }

    return out_file_path;
}

template<UEMeta::TopLevelDecl T>
void serialize(const T* msg, const std::shared_ptr<google::protobuf::Arena>& arena) {
    if (!msg) throw std::invalid_argument("Failed to serialize because msg is null!");
    if (!arena) throw std::invalid_argument("Failed to serialize because arena is null!");

    static const auto& cfg = UEMeta::Config::GetConfig();
    static const auto is_json = cfg.Format() == UEMeta::Config::SerializationFormat::json;
    static const auto open_mode = (is_json ? std::ios::out : std::ios::binary) | std::ios::trunc;
    static constexpr google::protobuf::json::PrintOptions json_options {.add_whitespace = true, .always_print_fields_with_no_presence = true };

    // Each write keeps the arena alive with thread-safe ownership across async tasks.
    auto serialize_fn = [msg, arena] {
        auto out_file_path = getSerializationPath(msg);
        std::ofstream out_file(out_file_path, open_mode);
        if (!out_file) {
            throw std::runtime_error("Failed to open file for writing!");
        }

        if (is_json) {
            thread_local std::string buffer{};
            buffer.clear();
            if (google::protobuf::util::MessageToJsonString(*msg, &buffer, json_options).ok()) {
                out_file << buffer;
            }
            else {
                UEM_ERROR("Failed to write to file: {}", out_file_path.string());
            }
        }
        else if (!msg->SerializeToOstream(&out_file)) {
            UEM_ERROR("Failed to write to file: {}", out_file_path.string());
        }

        out_file.close();
        (void)arena; // ensure the compiler doesn't do any weird optimizations with arena
    };

    if (cfg.SyncSerialization()) {
        serialize_fn();
    }

    else {
        UEMeta::DeclDb::serialization_pool.detach_task([serialize_fn = std::move(serialize_fn)] {
            try {
                serialize_fn();
            }
            catch (const std::exception& e) {
                UEM_ERROR("Failed to serialize declaration: {}", e.what());
            }
            catch (...) {
                UEM_ERROR("Failed to serialize declaration with unknown exception!");
            }
        });
    }
}
