#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "DeclWrapper.hpp"
#include "clang/AST/RecordLayout.h"

namespace UEMeta {
    /**
     * Serializes a Clang class, struct or union and the declarations it owns.
     * RecordDecl is Clang's common record node; C++ records are CXXRecordDecl subclasses.
     * The implementation's opening comments explain the AST shapes and anonymous-member walk.
     *
     * Ownership follows TopLevel.proto: named nested types have separate identities; an
     * unnamed type declared with a field belongs inside that field's TypeRefOrAnon; members
     * of anonymous storage with no source declarator are raised into the containing record.
     * A file-scope anonymous union instead produces a VariableGroup of global variables.
     *
     * The AST is borrowed and must remain alive during serialization and subsequent DeclDb
     * use. The protobuf arena owns messages as a group with a shared allocation lifetime.
     * All returned protobuf pointers belong to the supplied arena: keep that arena
     * alive while using them, and do not delete the messages individually. Embedded types
     * and methods share it. Independently named nested types use separate arenas; their
     * persistence is still a TODO in handleRecord/handleEnum.
     */
    class RecordDeclWrapper final : public DeclWrapper<clang::RecordDecl> {
    public:
        // Anonymous global unions need a group result to preserve is_global_union.
        using SerializeResult = std::variant<ParserTypes::TLRecordDeclaration*,
                                             ParserTypes::TLEnumDeclaration*,
                                             ParserTypes::VariableGroup*>;

        explicit RecordDeclWrapper(const clang::RecordDecl* decl,
                                   const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper(decl, arena) {}

        /**
         * Returns no payload for a forward declaration, one record for an ordinary or
         * field-owned definition, or one VariableGroup for a file-scope anonymous union.
         * The enum alternative is part of the result vocabulary, not currently emitted here.
         * Nested semantically anonymous records must go through their owner's extraction
         * path, not a standalone call to serialize().
         *
         * This is a single-pass operation, not a pure/repeatable conversion: despite const,
         * it allocates messages and updates DeclDb's identities, forwards and visited set.
         * Callers are responsible for avoiding duplicate serialization of an occurrence.
         */
        [[nodiscard]] std::vector<SerializeResult> serialize() const;

    private:
        // A union field needs global-variable metadata without becoming a VarDecl identity.
        class GlobalUnionFieldWrapper;

        [[nodiscard]] std::string computeFQN() const;
        [[nodiscard]] Hash computeDeclIdWithTemplateDetails(std::string_view fqn,
                                                            ParserTypes::TLRecordDeclaration* p_msg) const;
        [[nodiscard]] const clang::ASTRecordLayout* getLayout(const clang::RecordDecl* record) const;
        [[nodiscard]] ParserTypes::AccessSpecifier getAccess(clang::AccessSpecifier access,
                                                           const clang::RecordDecl* record) const;

        // Recurse into anonymous storage in declaration order; p_msg stays the receiving record.
        // layout belongs to record, whose origin in p_msg is record_offset_bits (zero at the root).
        void handleMembers(const clang::RecordDecl* record, ParserTypes::TLRecordDeclaration* p_msg,
                           const clang::ASTRecordLayout* layout, std::optional<uint64_t> record_offset_bits,
                           clang::AccessSpecifier inherited_access) const;
        void handleField(const clang::FieldDecl* field, ParserTypes::Field* p_msg,
                         const clang::ASTRecordLayout* layout, std::optional<uint64_t> absolute_offset_bits,
                         clang::AccessSpecifier inherited_access) const;
        void handleStaticField(clang::VarDecl* field, ParserTypes::Field* p_msg) const;
        void handleMethod(clang::CXXMethodDecl* method, ParserTypes::TLRecordDeclaration* p_msg,
                          const clang::ASTRecordLayout* layout) const;
        void handleBase(const clang::CXXBaseSpecifier& base, ParserTypes::BaseSpecifier* p_msg,
                        const clang::ASTRecordLayout* layout) const;
        void handleRecord(clang::RecordDecl* record, ParserTypes::TLRecordDeclaration* p_msg) const;
        void handleEnum(clang::EnumDecl* enumeration, ParserTypes::TLRecordDeclaration* p_msg,
                        clang::AccessSpecifier inherited_access) const;

        // Visitation and identity are separate: only independently named definitions get hashes.
        // Returns true when this is a forward occurrence and no definition payload should be emitted.
        [[nodiscard]] bool handleForwardDeclaration(clang::TagDecl* tag) const;
        void addNestedHash(const ParserTypes::DeclarationMetadata& metadata,
                           ParserTypes::TLRecordDeclaration* p_msg) const;

        // Use an ordinary reference (full desugared spelling + underlying identity), or embed an unnamed type.
        void putFieldType(clang::QualType type, ParserTypes::VersionedTypeRefOrAnon* p_msg) const;
        void putFieldMetadata(const clang::NamedDecl* field, ParserTypes::Field* p_msg,
                              clang::AccessSpecifier access) const;
        void putInitializer(const clang::Expr* initializer, ParserTypes::VersionedString* p_msg) const;

        // A global anonymous union owns a single group, including recursively injected fields.
        [[nodiscard]] ParserTypes::VariableGroup* serializeGlobalUnion() const;
        void extractGlobalUnionFields(const clang::RecordDecl* record, ParserTypes::VariableGroup* p_msg) const;
    };
}
