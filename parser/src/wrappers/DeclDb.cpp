#include "UEMeta/wrappers/DeclDb.hpp"

#include "CLI/ConfigFwd.hpp"
#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "UEMeta/wrappers/FunctionDeclWrapper.hpp"
#include "UEMeta/wrappers/RecordDeclWrapper.hpp"
#include "UEMeta/wrappers/Utility.hpp"
#include "UEMeta/wrappers/VarDeclWrapper.hpp"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "google/protobuf/util/json_util.h"

llvm::DenseMap<const clang::Decl*, UEMeta::Hash>                UEMeta::DeclDb::decl_to_identity_map;
absl::flat_hash_map<UEMeta::Hash, const clang::Decl*>           UEMeta::DeclDb::identity_to_decl_map;
llvm::DenseMap<const clang::Decl*, llvm::SmallVector<uint64_t>> UEMeta::DeclDb::decl_to_forward_decl_occurrence_map;
llvm::DenseSet<const clang::Decl*>                              UEMeta::DeclDb::visited_decls;

static bool isDeclInFunctionOrMethod(const clang::Decl* decl);
static bool isDeclInSystemOrStdHeader(const clang::Decl* decl);

template <UEMeta::TagDeclDerived T>
static bool isTagAnonymousAndEmbedded(const T* decl) {
    if (!decl)
        return false;
    return decl->getName().empty() && decl->getTypedefNameForAnonDecl() == nullptr && decl->isEmbeddedInDeclarator() && !decl->isFreeStanding();
}

template <UEMeta::TemplateSpecializableDeclType T>
static bool isImplicitSpec(const T* decl) {
    if (!decl)
        return false;
    return decl->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation;
}

void UEMeta::DeclDb::addDeclIdentity(const clang::Decl* decl, const Hash& hash) {
    if (!decl)
        throw DeclException(decl, "Can't addDeclIdentity with null Decl pointer!");
    decl_to_identity_map.insert({decl, hash});
    identity_to_decl_map.insert({hash, decl});
    visited_decls.insert(decl);
}

UEMeta::DeclDb::QueryResult UEMeta::DeclDb::queryDeclIdentity(const clang::Decl* decl) {
    try {
        if (!decl)
            return false;
        // Function-template references can name an earlier redeclaration, while the maps use the definition.
        if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
            if (const auto* definition = function->getDefinition())
                decl = definition;
        }
        if (const auto decl_hash_it = decl_to_identity_map.find(decl); decl_hash_it != decl_to_identity_map.end()) {
            return decl_hash_it->second;
        }

        const auto& sm = decl->getASTContext().getSourceManager();
        if (const auto loc = sm.getSpellingLoc(decl->getLocation());
            decl->isInStdNamespace() || sm.isInSystemHeader(loc) || sm.isInSystemMacro(loc)) {
            auto file = sm.getFileEntryRefForID(sm.getFileID(loc));
            if (!file)
                return false;
            return llvm::sys::path::filename(file->getName());
        }

        if (const auto fwd_hash_it = decl_to_forward_decl_occurrence_map.find(decl); fwd_hash_it != decl_to_forward_decl_occurrence_map.end()) {
            return fwd_hash_it->second.back();
        }

        return false;
    }
    catch (...) {
        return std::monostate{};
    }
}

const clang::Decl* UEMeta::DeclDb::queryDecl(const Hash& hash) {
    if (!hash)
        return nullptr;
    const auto decl_it = identity_to_decl_map.find(hash);
    if (decl_it == identity_to_decl_map.end())
        return nullptr;
    return decl_it->second;
}

UEMeta::DeclDb::QueryResult UEMeta::DeclDb::queryType(clang::QualType type, clang::QualType* unwrapped_type) {
    if (unwrapped_type)
        *unwrapped_type = {};
    try {
        // Resolve aliases, then strip structural layers that have no declaration identity.
        // Callers keep the original QualType for spelling; only this lookup peels Node*[N] to Node.
        if (type.isNull())
            return false;
        type = type.getCanonicalType();
        while (true) {
            if (type->isPointerType() || type->isReferenceType()) {
                type = type->getPointeeType();
            }
            else if (const auto* array = llvm::dyn_cast<clang::ArrayType>(type.getTypePtr())) {
                type = array->getElementType();
            }
            else
                break;
        }
        if (unwrapped_type)
            *unwrapped_type = type;

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
                    specialization && !llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(specialization) &&
                    specialization->getSpecializationKind() != clang::TSK_ExplicitSpecialization) {
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
            if (const bool* known = std::get_if<bool>(&result); !known || *known)
                return result;
        }

        // Classify types without a known declaration only after identity lookup, so dependent
        // record identities are not hidden by the generic builtin/template marker.
        return type->isBuiltinType() || type->isDependentType() || type->isTemplateTypeParmType();
    }
    catch (...) {
        return std::monostate{};
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::EnumDecl* decl) {
    try {
        if (!decl)
            return;
        if (visited_decls.contains(decl))
            return;
        visited_decls.insert(decl);
        if (isDeclInSystemOrStdHeader(decl))
            return;
        if (isImplicitSpec(decl))
            return;
        // if this enum is completely anonymous and attached to a declarator (like a VarDecl or FieldDecl), we'll get it later
        if (isTagAnonymousAndEmbedded(decl))
            return;
        if (isDeclInFunctionOrMethod(decl))
            return;

        if (!decl->isThisDeclarationADefinition()) {
            if (auto* definition = decl->getDefinition()) {
                addForwardDeclaration(definition);
            }
            return;
        }

        const auto arena = std::make_shared<google::protobuf::Arena>();
        EnumDeclWrapper(decl, arena).toFile();
    }
    catch (DeclException<clang::EnumDecl>& de) {
        throw;
    }
    catch (std::exception& e) {
        throw DeclException(decl, "{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::VarDecl* decl) {
    try {
        if (!decl)
            return;
        if (visited_decls.contains(decl))
            return;
        visited_decls.insert(decl);
        if (isDeclInSystemOrStdHeader(decl))
            return;
        if (isDeclInSystemOrStdHeader(decl->getTemplateInstantiationPattern()))
            return;
        if (decl->isLocalVarDeclOrParm())
            return;
        if (decl->isCXXClassMember())
            return;
        if (isImplicitSpec(decl))
            return;
        if (isDeclInFunctionOrMethod(decl))
            return;

        // Extern variables remain ordinary variable metadata, not forward occurrences.
        const auto arena = std::make_shared<google::protobuf::Arena>();
        VarDeclWrapper(decl, arena).toFile();
    }
    catch (DeclException<clang::VarDecl>& de) {
        throw;
    }
    catch (std::exception& e) {
        throw DeclException(decl, "{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::RecordDecl* decl) {
    try {
        if (!decl || visited_decls.contains(decl))
            return;
        visited_decls.insert(decl);
        if (decl->isImplicit() || isDeclInSystemOrStdHeader(decl) || isDeclInFunctionOrMethod(decl))
            return;
        if (isTagAnonymousAndEmbedded(decl))
            return;

        // Implicit instantiations never produce top-level metadata.
        if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
            if (isDeclInSystemOrStdHeader(cxx->getTemplateInstantiationPattern()))
                return;
            if (isImplicitSpec(cxx))
                return;
        }

        // Semantically anonymous member storage is extracted by its nearest owning record.
        if (decl->isAnonymousStructOrUnion() && decl->getDeclContext()->getNonTransparentContext()->isRecord())
            return;

        if (!decl->isThisDeclarationADefinition()) {
            if (auto* definition = decl->getDefinition()) {
                addForwardDeclaration(definition);
            }
            return;
        }

        const auto arena = std::make_shared<google::protobuf::Arena>();
        RecordDeclWrapper(decl, arena).toFile();
    }
    catch (DeclException<clang::RecordDecl>& de) {
        throw;
    }
    catch (std::exception& e) {
        throw DeclException(decl, "{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::FunctionDecl* decl) {
    try {
        if (!decl)
            return;
        if (visited_decls.contains(decl))
            return;
        visited_decls.insert(decl);
        if (isDeclInSystemOrStdHeader(decl))
            return;
        if (isDeclInSystemOrStdHeader(decl->getTemplateInstantiationPattern()))
            return;
        if (decl->isCXXClassMember())
            return;
        if (isImplicitSpec(decl))
            return;
        if (isDeclInFunctionOrMethod(decl))
            return;

        // Defer prototypes to a known definition, preserving declarations whose bodies are outside this AST.
        if (!decl->isThisDeclarationADefinition()) {
            if (auto* definition = decl->getDefinition()) {
                addForwardDeclaration(definition);
                return;
            }
        }

        const auto arena = std::make_shared<google::protobuf::Arena>();
        FunctionDeclWrapper(decl, arena).toFile();
    }
    catch (DeclException<clang::FunctionDecl>& de) {
        throw;
    }
    catch (std::exception& e) {
        throw DeclException(decl, "{}", e.what());
    }
}

void UEMeta::DeclDb::addForwardDeclaration(clang::Decl* for_decl) {
    const auto* tag      = llvm::dyn_cast_or_null<clang::TagDecl>(for_decl);
    const auto* function = llvm::dyn_cast_or_null<clang::FunctionDecl>(for_decl);
    if (!(tag && tag->isThisDeclarationADefinition()) && !(function && function->isThisDeclarationADefinition())) {
        throw DeclException(for_decl, "Failed to addForwardDeclaration because the declaration is not a record, enum or function definition!");
    }
    if (const auto other_decls = decl_to_forward_decl_occurrence_map.find(for_decl); other_decls != decl_to_forward_decl_occurrence_map.end()) {
        other_decls->second.emplace_back(Detail::DeclWrapperStatics::allocateDeclOccurrence());
    }
    else {
        decl_to_forward_decl_occurrence_map.insert({for_decl, {Detail::DeclWrapperStatics::allocateDeclOccurrence()}});
    }
}

void UEMeta::DeclDb::addDeclarationAsVisited(clang::Decl* decl) {
    if (!decl) {
        throw DeclException(decl, "Failed to addDeclarationAsVisited because decl is null!");
    }

    visited_decls.insert(decl);
}

void UEMeta::DeclDb::awaitPendingSerializations() { return Detail::DeclWrapperStatics::awaitPendingSerializations(); }

void UEMeta::DeclDb::serializeForwardDeclarations() {
    google::protobuf::Arena arena;
    auto*                   p_msg = google::protobuf::Arena::Create<ParserTypes::ForwardDeclarationList>(&arena);
    for (auto& decl_list_pair : decl_to_forward_decl_occurrence_map) {
        if (auto hash = decl_to_identity_map.find(decl_list_pair.first); hash != decl_to_identity_map.end()) {
            auto* p_list = p_msg->add_forward_declarations();
            hash->second.putProtoHash(p_list->mutable_type_id());
            for (const unsigned long long occ_index : decl_list_pair.second) {
                p_list->add_occurrence_indices(occ_index);
            }
        }
        else {
            std::string              buffer;
            llvm::raw_string_ostream os(buffer);
            decl_list_pair.first->print(os);
            UEM_WARN("Failed to find definition hash for decl {}, but it was forward declared!", buffer);
        }
    }

    const auto&                                    cfg       = Config::getConfig();
    const auto                                     is_json   = cfg.getFormat() == Config::SerializationFormat::Json;
    const auto                                     open_mode = (is_json ? std::ios::out : std::ios::binary) | std::ios::trunc;
    constexpr google::protobuf::json::PrintOptions json_options{
        .add_whitespace                       = true,
        .always_print_fields_with_no_presence = true
    };
    const auto&   out_path = cfg.getOutputDirectory().getUnderlyingPath() / (is_json ? "fwd.decljson" : "fwd.declbin");
    // todo can move this part and below to its own function shared with DeclWrapperStatics
    std::ofstream out_file(out_path, open_mode);
    if (!out_file) {
        throw std::runtime_error("Failed to open file for writing!");
    }

    if (is_json) {
        thread_local std::string buffer{};
        buffer.clear();
        if (google::protobuf::util::MessageToJsonString(*p_msg, &buffer, json_options).ok()) {
            out_file << buffer;
        }
        else {
            UEM_ERROR("Failed to write to file: {}", out_path.string());
        }
    }
    else if (!p_msg->SerializeToOstream(&out_file)) {
        UEM_ERROR("Failed to write to file: {}", out_path.string());
    }

    out_file.close();
}

#ifdef UEM_TESTING
void UEMeta::DeclDb::reset() {
    decl_to_identity_map.clear();
    decl_to_forward_decl_occurrence_map.clear();
    identity_to_decl_map.clear();
    visited_decls.clear();
}
#endif

bool isDeclInFunctionOrMethod(const clang::Decl* decl) {
    if (!decl)
        return false;

    auto rec_is_in_function_or_method = [](this auto self, const clang::DeclContext* decl_context) {
        if (!decl_context)
            return false;
        if (decl_context->isFunctionOrMethod())
            return true;
        return self(decl_context->getParent());
    };

    return rec_is_in_function_or_method(decl->getDeclContext());
}

bool isDeclInSystemOrStdHeader(const clang::Decl* decl) {
    if (!decl)
        return false;
    if (decl->isInStdNamespace())
        return true;

    const auto& sm  = decl->getASTContext().getSourceManager();
    const auto  loc = sm.getSpellingLoc(decl->getLocation());
    return sm.isInSystemHeader(loc) || sm.isInSystemMacro(loc);
}
