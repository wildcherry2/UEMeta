#include "UEMeta/wrappers/DeclDb.hpp"

#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "UEMeta/wrappers/MessageAllocator.hpp"
#include "UEMeta/wrappers/VarDeclWrapper.hpp"

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

UEMeta::DeclDb::QueryResult UEMeta::DeclDb::queryType(const clang::QualType type) {
    try {
        if (type.isNull()) {
            return false;
        }

        if (type->isBuiltinType() || type->isDependentType() || type->isTemplateTypeParmType()) {
            return true;
        }

        if (const clang::TagDecl *as_tag = type->getAsTagDecl()) {
            return queryDeclIdentity(as_tag);
        }

        if (const clang::EnumDecl *as_enum = type->getAsEnumDecl()) {
            return queryDeclIdentity(as_enum);
        }

        return false;
    } catch (...) {
        return std::monostate{};
    }
}

void UEMeta::DeclDb::serializeIfNeeded(const clang::EnumDecl* decl) {
    try {
        if (!decl) return;
        if (isDeclInSystemOrStdHeader(decl)) return;
        // if this enum is completely anonymous and attached to a declarator (like a VarDecl or FieldDecl), we'll get it later
        if (isTagAnonymousAndEmbedded(decl)) return;
        if (isDeclInFunctionOrMethod(decl)) return;
        if (decl_to_identity_map.contains(decl)) return;

        if (!decl->isThisDeclarationADefinition()) {
            auto* def = decl->getDefinition();
            if (!def) {
                //todo log or something since this is an undefined enum
                return;
            }
            if (const auto other_decls = decl_to_forward_decl_occurrence_map.find(def); other_decls != decl_to_forward_decl_occurrence_map.end()) {
                other_decls->second.emplace_back(allocateDeclOccurrence());
            }
            else {
                decl_to_forward_decl_occurrence_map.insert({def, {std::variant<uint64_t, clang::Decl*>{allocateDeclOccurrence()}}});
            }
            return;
        }

        EnumDeclWrapper(decl).serialize(Config::GetConfig().OutputDirectory().UnderlyingPath());
    } catch (std::exception& e) {
        UEM_ERROR("{}", e.what());
    }
}

void UEMeta::DeclDb::serializeIfNeeded(clang::VarDecl *decl) {
    try {
        if (!decl) return;
        if (isDeclInSystemOrStdHeader(decl)) return;
        if (isDeclInSystemOrStdHeader(decl->getTemplateInstantiationPattern())) return;
        if (decl->isLocalVarDeclOrParm()) return;
        if (decl->isCXXClassMember() && decl->getStorageDuration() != clang::SD_Static) return;
        if (failsImplicitSpecOption(decl)) return;
        if (isDeclInFunctionOrMethod(decl)) return;

        VarDeclWrapper(decl).serialize(Config::GetConfig().OutputDirectory().UnderlyingPath());
    } catch (std::exception& e) {
        UEM_ERROR("{}", e.what());
    }
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
