#include "UEMeta/wrappers/DeclWrapper.hpp"

UEMeta::Hash::Hash(boost::hash2::xxh3_128& hasher) {
    boost::hash2::digest<16> result = hasher.result();
    const auto* values = reinterpret_cast<uint64_t*>(result.data());
    a = values[0];
    b = values[1];
}

void UEMeta::DeclWrapper::compute() {
    const clang::SourceManager& source_manager = context.getSourceManager();
    file_location = source_manager.getFilename(source_manager.getExpansionLoc(decl->getLocation()));
    if (const clang::RawComment* comment = context.getRawCommentForDeclNoCache(decl)) {
        documentation = comment->getRawText(source_manager);
    }

    occurrence_index = allocateDeclOccurrence();
    has_identity = hasIdentity();

    if (has_identity) {
        const clang::QualType qual_type = getQualType(context);
        if (qual_type.isNull()) {
            throw std::invalid_argument("Failed to construct QualType for TypedDeclWrapper!");
        }

        fqn = clang::TypeName::getFullyQualifiedName(qual_type, context, context.getPrintingPolicy(), true);
        if (fqn.empty()) {
            throw std::runtime_error("Failed to construct FQN!");
        }

        type_id = computeTypeId(fqn, context);
        if (type_id.a == 0 && type_id.b == 0) {
            throw std::runtime_error("Invalid type_id!");
        }
    }
}

const clang::NamedDecl * UEMeta::DeclWrapper::getUnderlyingDecl() const {
    return decl;
}

void UEMeta::DeclWrapper::addForwardDeclaration() {
    forward_declarations.push_back(allocateDeclOccurrence());
}

uint64_t UEMeta::DeclWrapper::allocateDeclOccurrence() {
    static uint64_t value = 0;
    return value++;
}

void UEMeta::DeclWrapper::addToContentHash(boost::hash2::xxh3_128& hash) const {
    boost::hash2::hash_append(hash, boost::hash2::endian::little, file_location);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, documentation);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, occurrence_index);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, type_id.a);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, type_id.b);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, has_identity);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, fqn);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, forward_declarations);
}

void UEMeta::DeclWrapper::addToMetadata(ParserTypes::DeclarationMetadata* metadata) const {
    metadata->set_is_anonymous(!has_identity);
    SetVersionedString(metadata->mutable_documentation(), documentation);
    SetVersionedString(metadata->mutable_file_path(), file_location);
    SetVersionedInteger(metadata->mutable_occurrence_index(), occurrence_index);
    SetVersionedUint64List(metadata->add_forward_declaration_occurrence_indices(), forward_declarations);
    if (has_identity) {
        metadata->set_qualified_name(fqn);
        ParserTypes::Hash* p_hash = metadata->mutable_type_id();
        p_hash->set_a(type_id.a);
        p_hash->set_b(type_id.b);
    }
}
