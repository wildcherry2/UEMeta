#include "UEMeta/wrappers/FunctionDeclWrapper.hpp"

#include "clang/AST/GlobalDecl.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"

ParserTypes::MemberFunction* UEMeta::MethodDeclWrapper::serialize(bool has_known_layout) const {
    // Allocate the method beside its owning record and populate shared function details.
    auto* p_msg = google::protobuf::Arena::Create<ParserTypes::MemberFunction>(arena.get());
    p_msg->set_name(computeName());
    computeDeclIdWithTemplateDetails(computeFQN(), p_msg->mutable_common()).putProtoHash(p_msg->mutable_func_id());
    putFunctionCommon(p_msg->mutable_common());

    // Access and qualifiers are properties of the member, not independently tracked declarations.
    SetVersioned(p_msg->mutable_access(),
        decl->getAccess() == clang::AS_public ? ParserTypes::ACCESS_SPECIFIER_PUBLIC
        : decl->getAccess() == clang::AS_protected ? ParserTypes::ACCESS_SPECIFIER_PROTECTED
        : decl->getAccess() == clang::AS_private || decl->getParent()->isClass()
            ? ParserTypes::ACCESS_SPECIFIER_PRIVATE : ParserTypes::ACCESS_SPECIFIER_PUBLIC);
    SetVersionedBool(p_msg->mutable_is_const(), decl->isConst());
    SetVersionedBool(p_msg->mutable_is_volatile(), decl->isVolatile());
    SetVersionedBool(p_msg->mutable_is_deleted(), decl->isDeleted());
    SetVersioned(p_msg->mutable_virtuality(),
        decl->isPureVirtual() ? ParserTypes::FUNCTION_VIRTUALITY_PURE
        : decl->isVirtual() ? ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL
        : ParserTypes::FUNCTION_VIRTUALITY_NONE);

    // Only materialized layouts can supply ABI-dependent vtable locations.
    if (has_known_layout && decl->isVirtual()) {
        putVTableDetails(p_msg);
    }
    return p_msg;
}

void UEMeta::MethodDeclWrapper::putVTableDetails(ParserTypes::MemberFunction* p_msg) const {
    // Preserve the existing Microsoft ABI support boundary.
    auto* vtable = llvm::dyn_cast<clang::MicrosoftVTableContext>(getASTContext().getVTableContext());
    if (!vtable) {
        throw std::runtime_error("Itanium (Linux) ABI not supported yet!");
    }

    // Destructors occupy the ABI's deleting-destructor entry, not the complete-destructor entry.
    clang::GlobalDecl global_decl;
    if (const auto* destructor = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
        const auto destructor_kind = getASTContext().getTargetInfo().emitVectorDeletingDtors(getASTContext().getLangOpts())
            ? clang::Dtor_VectorDeleting : clang::Dtor_Deleting;
        global_decl = clang::GlobalDecl(destructor, destructor_kind);
    }
    else {
        global_decl = clang::GlobalDecl(decl);
    }

    // Clang reports a virtual-base vfptr relative to that base; translate it to the owning record.
    const auto location = vtable->getMethodVFTableLocation(global_decl);
    auto offset = location.VFPtrOffset;
    if (location.VBase) {
        offset += getASTContext().getASTRecordLayout(decl->getParent()).getVBaseClassOffset(location.VBase);
    }
    SetVersionedInteger(p_msg->mutable_vtable_index(), location.Index);
    SetVersionedInteger(p_msg->mutable_vtable_offset(), offset.getQuantity());
}
