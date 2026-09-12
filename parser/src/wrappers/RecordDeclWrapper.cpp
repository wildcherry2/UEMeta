#include "UEMeta/wrappers/RecordDeclWrapper.hpp"

#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "UEMeta/wrappers/FunctionDeclWrapper.hpp"
#include "boost/hash2/hash_append.hpp"
#include "clang/AST/DeclTemplate.h"
#include "llvm/ADT/StringExtras.h"

/*
 * Clang's AST (abstract syntax tree) contains both source declarations and compiler-created
 * declarations. A Decl is one declaration; a DeclContext owns a list of declarations, exposed
 * by decls(). A RecordDecl is both: the declaration of a class/struct/union and the container
 * for its members. llvm::dyn_cast<T>(decl) returns a T* if that node has the requested kind,
 * or nullptr otherwise. It does not change or resolve the declaration.
 *
 * handleMembers walks the declarations and dispatches to the field/method/type handlers.
 * FieldDecl means per-object storage; a static data member is a VarDecl instead, though
 * both become protobuf Fields here.
 * CXXMethodDecl goes to MethodDeclWrapper, which shares the free-function serialization
 * machinery. Implicit methods are retained only when virtual, including compiler-declared
 * destructors that override a virtual base destructor. Nested RecordDecl/EnumDecl nodes
 * describe types, not their eventual fields.
 * TemplateDecl can wrap the actual member node. Base classes are CXXBaseSpecifiers, not
 * Decls, so serialize() handles bases separately. Walking decls(), rather than separate
 * fields()/methods() lists, keeps the declarations together in their encountered order.
 *
 * "Anonymous" has two different meanings here (see the opening comments in TopLevel.proto):
 *
 *   struct Outer {
 *     union { int raised; };             // Semantically anonymous: Outer has field raised.
 *     struct { int retained; } value;    // Syntactically anonymous only: Outer has field value.
 *   };
 * A "declarator" is the part naming the object that uses the type: value in this example.
 *
 * In the first case Clang puts THREE kinds of node into Outer::decls():
 *   RecordDecl (unnamed union, containing the real FieldDecl raised)
 *   FieldDecl (implicit, unnamed storage for that union inside Outer)
 *   IndirectFieldDecl raised (a name injected into Outer, pointing through storage to raised)
 * The synthetic storage is recognized by FieldDecl::isAnonymousStructOrUnion(). It is not
 * itself an output Field: we recurse into its record and raise the real fields from there.
 * The indirect declaration only exposes a name for lookup, so the implicit-code filter
 * skips it to avoid duplicates. Unnamed bitfields have no injected name but are ordinary real
 * FieldDecls in the same recursive walk. "Implicit" means compiler-created, not layout-free:
 * every FieldDecl is handled before the implicit-code filter, including generated storage.
 *
 * In the second case value is an explicit, named FieldDecl. Its unnamed RecordDecl is
 * serialized inside value.type_ref. retained stays inside that record, not inside Outer.
 * If that embedded record contains semantic anonymous storage, it becomes the receiving
 * scope for those members: "nearest non-semantically-anonymous" does NOT mean "nearest
 * record with its own name". Each wrapper's output record fixes that receiving scope.
 * Clang's getNonTransparentContext() is not a general anonymous-record flattening operation;
 * we use it only for the file-context check, and recurse through storage fields for raising.
 *
 * There are also two separate kinds of DeclDb bookkeeping. Marking a node visited prevents
 * the outer AST visitor from serializing something already consumed here. Only record/enum
 * nodes need that tracking in this wrapper: fields, methods, enumerators and template wrappers
 * are not independent top-level output candidates. Giving a node an identity makes it a
 * reference target. A named record's fully qualified name (FQN), together with
 * its template details, determines its identity. Registering it before reading the members
 * allows self-references without a later repair pass.
 *
 * Schema helpers SetVersioned* and putTypeRef attach values to the current configured source
 * version. A missing optional layout value means "unknown", not zero. Record size/alignment
 * and base offsets use Clang's character units (bytes on the supported targets); field
 * offsets and widths use BITS. Source order must never be inferred from these offsets:
 * union members can share an offset, and zero-width bitfields still have a source position.
 * ABI-added slots (such as vptr/vbptr) and padding are not ordinary FieldDecls. Clang's
 * layout accounts for them, but this declaration walk does not emit separate Fields for them.
 */

// Adapt a FieldDecl to variable-shaped output without constructing a fake Clang VarDecl.
// For namespace N { static union { int x; }; }, the output identity is N::x, not an
// identity for the anonymous union. The outer owner supplies the receiving context and
// arena even when this leaf was reached through several anonymous storage records.
class UEMeta::RecordDeclWrapper::GlobalUnionFieldWrapper final : public DeclWrapper<clang::FieldDecl> {
public:
    GlobalUnionFieldWrapper(const clang::FieldDecl* field, const RecordDeclWrapper& owner)
        : DeclWrapper(field, owner.arena), owner(owner) {}

    [[nodiscard]] ParserTypes::TLGlobalVariableDeclaration* serialize() const {
        // Use the outer union's declared context so anonymous storage never enters the FQN.
        auto* p_msg = google::protobuf::Arena::Create<ParserTypes::TLGlobalVariableDeclaration>(arena.get());
        std::string fqn;
        llvm::raw_string_ostream os{fqn};
        putContextFQN(os, owner.decl);
        decl->printName(os, decl->getASTContext().getPrintingPolicy());
        boost::hash2::xxh3_128 hasher;
        hasher.update(fqn.data(), fqn.size());
        putMetadata(p_msg->mutable_metadata(), true, fqn, Hash{hasher});

        // These values have static storage but are not constexpr merely because their type is const.
        p_msg->set_is_anon_union_value(true);
        SetVersioned(p_msg->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
        SetVersioned(p_msg->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_NONE);
        owner.putFieldType(decl->getType(), p_msg->mutable_type_ref());
        if (const auto* initializer = decl->getInClassInitializer()) {
            owner.putInitializer(initializer, p_msg->mutable_default_value());
        }
        //todo register variable with DeclDb
        return p_msg;
    }

private:
    const RecordDeclWrapper& owner;
};

// Entry-point phases: classify the occurrence, establish identity, emit available layout,
// then consume bases and members. We never jump ahead to serialize a later definition.
std::vector<UEMeta::RecordDeclWrapper::SerializeResult> UEMeta::RecordDeclWrapper::serialize() const {
    // Forward occurrences are recorded without consuming the eventual definition's visit.
    if (!decl) throw std::invalid_argument("Cannot serialize a null record declaration!");
    if (handleForwardDeclaration(const_cast<clang::RecordDecl*>(decl))) return {};

    // File-scope anonymous unions inject static variables into their enclosing namespace.
    if (decl->isUnion() && decl->isAnonymousStructOrUnion()
        && decl->getDeclContext()->getNonTransparentContext()->isFileContext()) {
        return {serializeGlobalUnion()};
    }
    if (decl->isAnonymousStructOrUnion()) {
        throw std::invalid_argument("Nested anonymous storage must be extracted by its owning record!");
    }

    // Register named identities before member serialization so self-references resolve in this pass.
    // hasNameForLinkage(), unlike a nonempty source name, includes typedef-named anonymous
    // records. An embedded unnamed type gets normal contents but no standalone FQN/decl_id.
    auto* p_msg = google::protobuf::Arena::Create<ParserTypes::TLRecordDeclaration>(arena.get());
    const bool has_identity = decl->hasNameForLinkage();
    if (has_identity) {
        const std::string fqn = computeFQN();
        const Hash identity = computeDeclIdWithTemplateDetails(fqn, p_msg);
        putMetadata(p_msg->mutable_metadata(), true, fqn, identity);
        DeclDb::addDeclIdentity(const_cast<clang::RecordDecl*>(decl), identity);
    }
    else {
        putMetadata(p_msg->mutable_metadata(), false);
    }

    // Layout is optional for dependent records; kind and members are always retained.
    p_msg->set_kind(decl->isClass() ? ParserTypes::RECORD_KIND_CLASS
        : decl->isStruct() ? ParserTypes::RECORD_KIND_STRUCT : ParserTypes::RECORD_KIND_UNION);
    const auto* layout = getLayout(decl);
    if (layout) {
        SetVersionedInteger(p_msg->mutable_size_bytes(), layout->getSize().getQuantity());
        SetVersionedInteger(p_msg->mutable_align_bytes(), layout->getAlignment().getQuantity());
    }

    // Clang stores bases outside decls(), so only base specifiers require their own loop.
    if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        for (const auto& base : cxx->bases()) {
            handleBase(base, p_msg->add_bases(), layout);
        }
    }
    handleMembers(decl, p_msg, layout, uint64_t{0}, clang::AS_none);
    return {p_msg};
}

std::string UEMeta::RecordDeclWrapper::computeFQN() const {
    // Canonical tag types also give typedef-named anonymous records their C++ name.
    const clang::QualType type = getASTContext().getCanonicalTagType(decl);
    return clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
}

UEMeta::Hash UEMeta::RecordDeclWrapper::computeDeclIdWithTemplateDetails(
    std::string_view fqn, ParserTypes::TLRecordDeclaration* p_msg) const {
    // Record identity combines the FQN with the same template fragments used by the other wrappers.
    boost::hash2::xxh3_128 hasher;
    boost::hash2::hash_append(hasher, boost::hash2::endian::little, fqn);
    const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
    if (!cxx) return Hash{hasher};

    // For template<class T> struct Box, Box is the primary template and T is a parameter.
    // Box<int> supplies an argument; template<class T> struct Box<T*> is a partial
    // specialization, with both a remaining parameter list and specialized arguments.
    const auto* primary = cxx->getDescribedClassTemplate();
    const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(cxx);
    const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(cxx);
    const auto* parameters = primary ? primary->getTemplateParameters()
        : partial ? partial->getTemplateParameters() : nullptr;
    const auto* arguments = specialization ? &specialization->getTemplateArgs() : nullptr;
    if (!parameters && !arguments) return Hash{hasher};

    // Query the templated record definition, never the ClassTemplateDecl or a dependent QualType.
    // DeclDb keys identities by the serialized declaration node. An instantiation can refer
    // back to a partial-specialization pattern, not necessarily to the primary template.
    DeclDb::QueryResult primary_id{false};
    if (specialization) {
        const clang::CXXRecordDecl* target = specialization->getSpecializedTemplate()->getTemplatedDecl();
        if (!partial && specialization->getSpecializationKind() != clang::TSK_ExplicitSpecialization) {
            if (const auto* pattern = specialization->getTemplateInstantiationPattern()) {
                target = pattern;
            }
        }
        primary_id = DeclDb::queryDeclIdentity(target->getDefinition() ? target->getDefinition() : target);
    }

    // Serialize template details once and hash their identity fragments in declaration order.
    std::vector<AnyString> template_identity;
    putTemplateDetails(parameters, p_msg->mutable_template_details(), arguments, primary_id, &template_identity);
    for (const AnyString& fragment : template_identity) {
        if (const auto* ref = std::get_if<llvm::StringRef>(&fragment)) {
            hasher.update(ref->data(), ref->size());
        }
        else if (const auto* view = std::get_if<std::string_view>(&fragment)) {
            hasher.update(view->data(), view->size());
        }
        else if (const auto* string = std::get_if<std::string>(&fragment)) {
            hasher.update(string->data(), string->size());
        }
    }
    return Hash{hasher};
}

const clang::ASTRecordLayout* UEMeta::RecordDeclWrapper::getLayout(const clang::RecordDecl* record) const {
    // "Dependent" means some information still depends on template parameters, e.g. T field
    // or unsigned bits : N. The source declaration can be serialized before its size is known.
    // Dependent contexts include dependent fields, bases, bit widths and alignas expressions.
    // Leave their optional layout fields unset rather than asking Clang to instantiate a layout.
    if (!record->isCompleteDefinition() || record->isInvalidDecl() || record->isDependentContext()) {
        return nullptr;
    }
    return &getASTContext().getASTRecordLayout(record);
}

ParserTypes::AccessSpecifier UEMeta::RecordDeclWrapper::getAccess(
    clang::AccessSpecifier access, const clang::RecordDecl* record) const {
    // Clang supplies explicit/default C++ access; C records need the public fallback.
    switch (access) {
        case clang::AS_public: return ParserTypes::ACCESS_SPECIFIER_PUBLIC;
        case clang::AS_protected: return ParserTypes::ACCESS_SPECIFIER_PROTECTED;
        case clang::AS_private: return ParserTypes::ACCESS_SPECIFIER_PRIVATE;
        case clang::AS_none:
            return record->isClass() ? ParserTypes::ACCESS_SPECIFIER_PRIVATE : ParserTypes::ACCESS_SPECIFIER_PUBLIC;
    }
    throw std::runtime_error("Unknown record access specifier!");
}

/**
 * Walk each record's immediate declarations in order, recursing only through semantic
 * anonymous storage. The record's declaration list contains both named and unnamed fields;
 * IndirectFieldDecls are redundant names for some of those fields and are never serialized.
 *
 * Recursion happens at the storage FieldDecl's position, before the next enclosing member.
 * A body { unsigned : 1; int a; unsigned : 2; int b; unsigned : 0; } therefore appends
 * [:1, a, :2, b, :0] directly, even inside multiple anonymous records. Leading, interleaved
 * and trailing unnamed bitfields all use handleField, with Field.name left absent.
 * There is no second traversal for padding, injection matching, sorting or output post-pass.
 *
 * record and layout describe the CURRENT storage scope; record_offset_bits is its origin
 * in the RECEIVING record, in bits. The initial origin is zero. Each anonymous descent adds
 * its storage field's local offset, and each emitted field adds its own local offset:
 * storage at bit 64, inner storage at local bit 16, leaf at local bit 3 -> output offset 83.
 * Missing layout propagates an unknown offset; an optional containing zero is still known.
 *
 * this and p_msg stay fixed during anonymous descent, so all raised members belong to the
 * nearest non-semantically-anonymous record. A named field with an unnamed type is NOT
 * storage to descend into here: putFieldType gives its type a separate wrapper and output.
 * inherited_access carries the outer anonymous storage's visibility to the raised fields.
 */
void UEMeta::RecordDeclWrapper::handleMembers(
    const clang::RecordDecl* record, ParserTypes::TLRecordDeclaration* p_msg,
    const clang::ASTRecordLayout* layout, std::optional<uint64_t> record_offset_bits,
    clang::AccessSpecifier inherited_access) const {
    for (clang::Decl* member : record->decls()) {
        // Keep every storage field, including compiler-generated fields and unnamed bitfields.
        // isImplicit() describes a declaration's origin, not whether it contributes to layout.
        if (auto* field = llvm::dyn_cast<clang::FieldDecl>(member)) {
            const bool is_anonymous_storage = field->isAnonymousStructOrUnion();

            // Both emitted fields and anonymous storage need their offset in the receiving
            // record. Compute it before changing layouts; unknown parent offsets stay unknown.
            std::optional<uint64_t> offset;
            if (layout && record_offset_bits) {
                offset = *record_offset_bits + layout->getFieldOffset(field->getFieldIndex());
            }

            // Storage itself is not an output Field. Recurse immediately at this source
            // position, keeping the receiver/arena and using the child's layout and origin.
            if (is_anonymous_storage) {
                auto* anonymous = field->getType()->getAsRecordDecl();
                DeclDb::addDeclarationAsVisited(anonymous);
                const auto access = inherited_access != clang::AS_none ? inherited_access : field->getAccess();
                handleMembers(anonymous, p_msg, getLayout(anonymous), offset, access);
            }
            else {
                // Named fields and unnamed/zero-width bitfields append in the same source order.
                handleField(field, p_msg->add_fields(), layout, offset, inherited_access);
            }
            continue;
        }

        // Templates wrap their actual declaration. Unwrap supported member templates for
        // dispatch below; aliases and other template kinds do not introduce serialized members.
        // Non-static fields cannot be templates, so no further FieldDecl cast is needed.
        if (auto* member_template = llvm::dyn_cast<clang::TemplateDecl>(member)) {
            // Compiler-generated template wrappers remain outside the source-member walk.
            if (member_template->isImplicit()) continue;
            if (!llvm::isa<clang::ClassTemplateDecl, clang::FunctionTemplateDecl, clang::VarTemplateDecl>(member_template)) {
                continue;
            }
            member = member_template->getTemplatedDecl();
        }

        // Keep explicit methods and implicit virtual methods, but omit implicit non-virtual ones.
        // isVirtual(), unlike isVirtualAsWritten(), also recognizes generated overriding destructors.
        if (auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(member)) {
            if (!method->isImplicit() || method->isVirtual()) {
                handleMethod(method, p_msg, layout);
            }
            continue;
        }

        // Fields and virtual methods have already been handled. Remaining implicit nodes,
        // such as IndirectFieldDecl lookup aliases and injected class names, are not output.
        if (member->isImplicit()) continue;

        // Dispatch remaining members; nested type handlers track their record/enum visits.
        if (auto* field = llvm::dyn_cast<clang::VarDecl>(member)) {
            if (field->isStaticDataMember()) handleStaticField(field, p_msg->add_fields());
        }
        else if (auto* nested_record = llvm::dyn_cast<clang::RecordDecl>(member)) {
            handleRecord(nested_record, p_msg);
        }
        else if (auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(member)) {
            handleEnum(enumeration, p_msg, inherited_access);
        }
        // Aliases, using declarations, access labels and friends do not introduce owned members here.
    }
}

// Common output for direct/raised fields, including unnamed and zero-width bitfields.
// absolute_offset_bits is already relative to p_msg's owning record, NOT necessarily
// field->getParent(). layout supplies local layout availability; do not add another offset
// here. An engaged optional containing 0 is a known offset, distinct from no layout.
void UEMeta::RecordDeclWrapper::handleField(
    const clang::FieldDecl* field, ParserTypes::Field* p_msg, const clang::ASTRecordLayout* layout,
    std::optional<uint64_t> absolute_offset_bits, clang::AccessSpecifier inherited_access) const {
    // Preserve field source metadata and translate anonymous storage access to the owning record.
    const auto access = inherited_access != clang::AS_none ? inherited_access : field->getAccess();
    putFieldMetadata(field, p_msg, access);
    putFieldType(field->getType(), p_msg->mutable_type_ref());
    SetVersionedBool(p_msg->mutable_is_mutable(), field->isMutable());
    SetVersionedBool(p_msg->mutable_is_bitfield(), field->isBitField());
    SetVersioned(p_msg->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);
    SetVersioned(p_msg->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_NONE);

    // Bit widths can be known even when the enclosing record's layout remains dependent.
    if (field->isBitField()) {
        if (!field->getBitWidth()->isValueDependent()) {
            SetVersionedInteger(p_msg->mutable_bit_width(), field->getBitWidthValue());
        }
    }
    else if (layout && !field->getType()->isIncompleteType()) {
        SetVersionedInteger(p_msg->mutable_bit_width(), getASTContext().getTypeSize(field->getType()));
    }
    // The walk supplies the offset in the receiving record, whether the field is direct or raised.
    if (absolute_offset_bits) {
        SetVersionedInteger(p_msg->mutable_offset_bits(), *absolute_offset_bits);
    }

    // In-class initializers are source expressions, including dependent expressions.
    if (const auto* initializer = field->getInClassInitializer()) {
        putInitializer(initializer, p_msg->mutable_default_value());
    }
}

void UEMeta::RecordDeclWrapper::handleStaticField(clang::VarDecl* field, ParserTypes::Field* p_msg) const {
    // Static data members stay in fields; the top-level variable path excludes class members.
    putFieldMetadata(field, p_msg, field->getAccess());
    putFieldType(field->getType(), p_msg->mutable_type_ref());
    SetVersionedBool(p_msg->mutable_is_mutable(), false);
    SetVersionedBool(p_msg->mutable_is_bitfield(), false);
    SetVersioned(p_msg->mutable_storage_class(), field->getTLSKind() != clang::VarDecl::TLS_None
        ? ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL : ParserTypes::VAR_STORAGE_CLASS_STATIC);
    SetVersioned(p_msg->mutable_constant_evaluation_kind(), field->isConstexpr()
        ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE);

    // A static member has a type size but no offset within an instance of its owning record.
    if (!field->getType()->isDependentType() && !field->getType()->isIncompleteType()) {
        SetVersionedInteger(p_msg->mutable_bit_width(), getASTContext().getTypeSize(field->getType()));
    }
    if (const auto* initializer = field->getInit()) {
        putInitializer(initializer, p_msg->mutable_default_value());
    }
}

void UEMeta::RecordDeclWrapper::handleMethod(
    clang::CXXMethodDecl* method, ParserTypes::TLRecordDeclaration* p_msg,
    const clang::ASTRecordLayout* layout) const {
    // Member functions share the record arena and are not candidates for free-function output.
    p_msg->mutable_methods()->AddAllocated(MethodDeclWrapper(method, arena).serialize(layout != nullptr));
}

void UEMeta::RecordDeclWrapper::handleBase(
    const clang::CXXBaseSpecifier& base, ParserTypes::BaseSpecifier* p_msg,
    const clang::ASTRecordLayout* layout) const {
    // A base is a specifier rather than a Decl; its type resolves through the same query rules as fields.
    const clang::QualType type = base.getType().getCanonicalType();
    std::string name = clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
    if (base.isPackExpansion()) name += "...";
    putTypeRef(name, DeclDb::queryType(type), p_msg->mutable_type_ref());
    SetVersioned(p_msg->mutable_access(), getAccess(base.getAccessSpecifier(), decl));
    SetVersionedBool(p_msg->mutable_is_virtual(), base.isVirtual());

    // Layout offsets use the actual base specialization, even when its identity refers to a template pattern.
    if (const auto* base_record = type->getAsCXXRecordDecl(); layout && base_record) {
        const auto offset = base.isVirtual() ? layout->getVBaseClassOffset(base_record)
                                             : layout->getBaseClassOffset(base_record);
        SetVersionedInteger(p_msg->mutable_offset(), offset.getQuantity());
    }
}

bool UEMeta::RecordDeclWrapper::handleForwardDeclaration(clang::TagDecl* tag) const {
    // A TagDecl is Clang's common base for record and enum declarations. "struct X;" and
    // "struct X { ... };" are different AST nodes for one type. getDefinition() can see the
    // latter even before this serialization walk reaches it: the parsed AST can already
    // contain later source declarations. Only record the forward occurrence now; do not serialize ahead.
    // DeclDb needs the definition pointer as its key, but visitation below applies to tag only.
    DeclDb::addDeclarationAsVisited(tag);
    if (tag->isThisDeclarationADefinition()) return false;
    if (auto* definition = tag->getDefinition()) {
        DeclDb::addForwardDeclaration(definition);
    }
    // TODO: DeclDb cannot yet register a forward whose tag has no definition in this translation unit.
    return true;
}

void UEMeta::RecordDeclWrapper::handleRecord(
    clang::RecordDecl* record, ParserTypes::TLRecordDeclaration* p_msg) const {
    if (handleForwardDeclaration(record)) return;

    // Seeing the type definition is not the same as seeing its storage. For an unnamed
    // record, defer output until the following FieldDecl identifies the case: a named
    // declarator embeds the type; implicit anonymous storage triggers recursive member extraction.
    if (!record->hasNameForLinkage()) return;

    // Named nested declarations get independent arenas and contribute only their own IDs to this record.
    // nested_hashes contains direct children, not a recursive list of all their descendants.
    // Copying these hashes does not preserve the nested payloads: the saving TODO below must
    // eventually retain/consume each nested arena before this local owner is destroyed.
    const auto nested_arena = boost::local_shared_ptr<google::protobuf::Arena>(new google::protobuf::Arena());
    const auto results = RecordDeclWrapper(record, nested_arena).serialize();
    for (const auto& result : results) {
        if (const auto* nested = std::get_if<ParserTypes::TLRecordDeclaration*>(&result)) {
            addNestedHash((*nested)->metadata(), p_msg);
        }
        else if (const auto* nested = std::get_if<ParserTypes::TLEnumDeclaration*>(&result)) {
            addNestedHash((*nested)->metadata(), p_msg);
        }
    }
    // TODO: Save the nested results together with nested_arena before releasing their arena here.
}

// Enum policy mirrors record ownership, but anonymous enum contents are constants rather
// than storage. Embedded unnamed enums stay with their declarator; freestanding unnamed
// enums contribute constexpr static Fields at this source position. Named enums are
// delegated to EnumDeclWrapper and linked through nested_hashes, not flattened here.
void UEMeta::RecordDeclWrapper::handleEnum(
    clang::EnumDecl* enumeration, ParserTypes::TLRecordDeclaration* p_msg,
    clang::AccessSpecifier inherited_access) const {
    if (handleForwardDeclaration(enumeration)) return;

    // A syntactically anonymous enum with a declarator belongs to that declarator's TypeRefOrAnon.
    if (!enumeration->hasNameForLinkage() && enumeration->isEmbeddedInDeclarator()
        && !enumeration->isFreeStanding()) return;
    if (!enumeration->hasNameForLinkage()) {
        // Semantically anonymous enumerators become static constexpr fields of the nearest owning record.
        const auto access = inherited_access != clang::AS_none ? inherited_access : enumeration->getAccess();
        clang::QualType type = enumeration->getIntegerType();
        if (type.isNull()) type = enumeration->getPromotionType();
        for (auto* enumerator : enumeration->enumerators()) {
            auto* p_field = p_msg->add_fields();
            putFieldMetadata(enumerator, p_field, access);
            putFieldType(type.isNull() ? enumerator->getType() : type, p_field->mutable_type_ref());
            p_field->set_is_anon_enum_value(true);
            SetVersionedBool(p_field->mutable_is_mutable(), false);
            SetVersionedBool(p_field->mutable_is_bitfield(), false);
            SetVersioned(p_field->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
            SetVersioned(p_field->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);

            // Dependent enumerators retain their initializer expression until their values are known.
            if (const auto* initializer = enumerator->getInitExpr(); initializer && initializer->isValueDependent()) {
                putInitializer(initializer, p_field->mutable_default_value());
            }
            else {
                SetVersionedString(p_field->mutable_default_value(), llvm::toString(enumerator->getInitVal(), 10));
            }
        }
        return;
    }

    // Delegate named enums to their wrapper and publish the returned identity for following members.
    const auto nested_arena = boost::local_shared_ptr<google::protobuf::Arena>(new google::protobuf::Arena());
    const auto result = EnumDeclWrapper(enumeration, nested_arena).serialize();
    const auto* nested = std::get_if<ParserTypes::TLEnumDeclaration*>(&result);
    if (!nested || !(*nested)->metadata().has_decl_id()) {
        throw std::runtime_error("A named nested enum did not produce an enum identity!");
    }
    Hash identity{};
    identity.a = (*nested)->metadata().decl_id().a();
    identity.b = (*nested)->metadata().decl_id().b();
    DeclDb::addDeclIdentity(enumeration, identity);
    addNestedHash((*nested)->metadata(), p_msg);
    // TODO: Save *nested together with nested_arena before releasing its arena here.
}

void UEMeta::RecordDeclWrapper::addNestedHash(
    const ParserTypes::DeclarationMetadata& metadata, ParserTypes::TLRecordDeclaration* p_msg) const {
    // Allocate a version only when a named nested declaration actually contributes an identity.
    if (!metadata.has_decl_id()) return;
    auto* hashes = p_msg->mutable_nested_hashes();
    if (hashes->versions_size() == 0) {
        hashes->add_versions()->add_source_versions(Config::GetConfig().Version());
    }
    hashes->mutable_versions(0)->add_value()->CopyFrom(metadata.decl_id());
}

// QualType carries a Clang type plus qualifiers such as const. Its canonical form removes
// typedef/using aliases. Type spelling and reference identity serve different purposes:
// for "const Node*", the ordinary TypeRef keeps that full type, but queries Node's identity,
// not a hash for the pointer type or for the field declaration itself.
//
// DeclDb::queryType unwraps pointer/reference/array layers and resolves instantiations to
// their source primary/partial specialization; explicit specializations keep their own
// declaration. Generated instantiation identities are never queried. Its QueryResult is NOT always a
// hash: it may be an earlier forward occurrence, a system-header name, a bool indicating
// builtin/dependent versus unknown, or monostate for failure. putTypeRef encodes these
// alternatives; a missing hash alone is not an error and must not invent a new identity.
//
// The other TypeRefOrAnon branch owns an entire unnamed record/enum message instead of
// referencing a standalone identity. It uses the same arena as the field. Peeling the
// declarator layers below detects that case; the full-spelling TypeRef path is only used
// when we do not select one of these embedded-message branches.
void UEMeta::RecordDeclWrapper::putFieldType(
    clang::QualType type, ParserTypes::VersionedTypeRefOrAnon* p_msg) const {
    if (type.isNull()) throw std::runtime_error("Cannot serialize a field without a type!");

    // Keep cvref/pointer/array spelling in TypeRef while fully resolving alias sugar.
    type = type.getCanonicalType();
    const auto query = DeclDb::queryType(type);
    if (std::holds_alternative<std::monostate>(query)) {
        throw std::runtime_error("DeclDb failed to query a record member type!");
    }
    auto* version = p_msg->add_versions();
    version->add_source_versions(Config::GetConfig().Version());
    auto* value = version->mutable_value();

    // Anonymous embedded types belong to their field even when a dependent-type query returns true.
    clang::QualType underlying = type;
    while (true) {
        if (underlying->isPointerType() || underlying->isReferenceType()) underlying = underlying->getPointeeType();
        else if (const auto* array = getASTContext().getAsArrayType(underlying)) underlying = array->getElementType();
        else break;
    }
    if (auto* tag = underlying->getAsTagDecl(); tag && !tag->hasNameForLinkage()
        && tag->isEmbeddedInDeclarator() && !tag->isFreeStanding()) {
        if (auto* record = llvm::dyn_cast_or_null<clang::RecordDecl>(tag->getDefinition())) {
            DeclDb::addDeclarationAsVisited(record);
            const auto results = RecordDeclWrapper(record, arena).serialize();
            if (results.size() != 1 || !std::holds_alternative<ParserTypes::TLRecordDeclaration*>(results.front())) {
                throw std::runtime_error("An embedded anonymous record did not produce a single record!");
            }
            value->set_allocated_anon_record(std::get<ParserTypes::TLRecordDeclaration*>(results.front()));
            return;
        }
        if (auto* enumeration = llvm::dyn_cast_or_null<clang::EnumDecl>(tag->getDefinition())) {
            DeclDb::addDeclarationAsVisited(enumeration);
            const auto result = EnumDeclWrapper(enumeration, arena).serialize();
            const auto* nested = std::get_if<ParserTypes::TLEnumDeclaration*>(&result);
            if (!nested) throw std::runtime_error("An embedded anonymous enum did not produce an enum!");
            value->set_allocated_anon_enum(*nested);
            return;
        }
    }

    // Every remaining query alternative is preserved: hash, forward occurrence, header, builtin or unknown.
    putTypeRef(clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true),
               query, value->mutable_type_ref());
}

void UEMeta::RecordDeclWrapper::putFieldMetadata(
    const clang::NamedDecl* field, ParserTypes::Field* p_msg, clang::AccessSpecifier access) const {
    // Unnamed bitfields leave the optional name absent; all fields retain documentation and owning-scope access.
    if (field->getDeclName()) p_msg->set_name(field->getNameAsString());
    SetVersioned(p_msg->mutable_access(), getAccess(access, decl));
    if (const auto* comment = getASTContext().getRawCommentForAnyRedecl(field)) {
        SetVersionedString(p_msg->mutable_documentation(), comment->getRawText(getASTContext().getSourceManager()));
    }
}

void UEMeta::RecordDeclWrapper::putInitializer(
    const clang::Expr* initializer, ParserTypes::VersionedString* p_msg) const {
    // Match VarDeclWrapper's source-level initializer serialization.
    std::string out;
    llvm::raw_string_ostream os{out};
    initializer->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
    SetVersionedString(p_msg, out);
}

ParserTypes::VariableGroup* UEMeta::RecordDeclWrapper::serializeGlobalUnion() const {
    // The group retains the union relationship while each named field gets variable metadata.
    auto* group = google::protobuf::Arena::Create<ParserTypes::VariableGroup>(arena.get());
    group->set_is_global_union(true);
    extractGlobalUnionFields(decl, group);
    return group;
}

// Separate from record-member extraction: this output is a group of named global values,
// not a Fields list. Recursing over real storage fields suffices; repeated injection nodes
// are skipped, and unnamed bitfields cannot become independently named global variables.
// No anonymous-storage offsets are needed because TLGlobalVariableDeclaration has none.
void UEMeta::RecordDeclWrapper::extractGlobalUnionFields(
    const clang::RecordDecl* record, ParserTypes::VariableGroup* p_msg) const {
    for (clang::Decl* member : record->decls()) {
        // Anonymous storage recursively injects values; unnamed bitfields are padding, not variables.
        if (auto* field = llvm::dyn_cast<clang::FieldDecl>(member)) {
            if (field->isAnonymousStructOrUnion()) {
                auto* anonymous = field->getType()->getAsRecordDecl();
                DeclDb::addDeclarationAsVisited(anonymous);
                extractGlobalUnionFields(anonymous, p_msg);
            }
            else if (!field->getDeclName().isEmpty()) {
                p_msg->mutable_variables()->AddAllocated(GlobalUnionFieldWrapper(field, *this).serialize());
            }
        }
        else if (auto* tag = llvm::dyn_cast<clang::TagDecl>(member)) {
            // Embedded type definitions are owned by their values; mark their declarations now.
            (void)handleForwardDeclaration(tag);
        }
    }
}
