#include "UEMeta/clang/wrappers/FunctionDeclWrapper.hpp"

#include "UEMeta/utility/DeclException.hpp"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "UEMeta/clang/ReflectionDb.hpp"

ParserTypes::MemberFunction* UEMeta::MethodDeclWrapper::serialize(bool has_known_layout, const Hash& owner_id) const {
    // Allocate the method beside its owning record and populate shared function details.
    auto* p_msg = google::protobuf::Arena::Create<ParserTypes::MemberFunction>(arena.get());
    p_msg->set_name(computeName());
    const Hash func_id = computeDeclIdWithTemplateDetails(computeFQN(), p_msg->mutable_common());
    func_id.putProtoHash(p_msg->mutable_func_id());
    putFunctionCommon(p_msg->mutable_common());

    // Access and qualifiers are properties of the member, not independently tracked declarations.
    setVersioned(p_msg->mutable_access(), decl->getAccess() == clang::AS_public      ? ParserTypes::ACCESS_SPECIFIER_PUBLIC
                                          : decl->getAccess() == clang::AS_protected ? ParserTypes::ACCESS_SPECIFIER_PROTECTED
                                          : decl->getAccess() == clang::AS_private || decl->getParent()->isClass()
                                              ? ParserTypes::ACCESS_SPECIFIER_PRIVATE
                                              : ParserTypes::ACCESS_SPECIFIER_PUBLIC);
    setVersionedBool(p_msg->mutable_is_const(), decl->isConst());
    setVersionedBool(p_msg->mutable_is_volatile(), decl->isVolatile());
    setVersionedBool(p_msg->mutable_is_deleted(), decl->isDeleted());
    setVersioned(p_msg->mutable_virtuality(), decl->isPureVirtual() ? ParserTypes::FUNCTION_VIRTUALITY_PURE
                                              : decl->isVirtual()   ? ParserTypes::FUNCTION_VIRTUALITY_VIRTUAL
                                                                    : ParserTypes::FUNCTION_VIRTUALITY_NONE);

    // Only materialized layouts can supply ABI-dependent virtual dispatch data.
    if (has_known_layout && decl->isVirtual()) {
        putVirtualDispatchInfo(p_msg);
    }

    ReflectionDb::registerReflectable(decl, owner_id, func_id);
    return p_msg;
}

void UEMeta::MethodDeclWrapper::putVirtualDispatchInfo(ParserTypes::MemberFunction* p_msg) const {
    // Preserve the existing Microsoft ABI support boundary.
    auto* vtable = llvm::dyn_cast<clang::MicrosoftVTableContext>(getASTContext().getVTableContext());
    if (!vtable) {
        throw DeclException(decl, "Itanium (Linux) ABI not supported yet!");
    }

    // Destructors occupy the ABI's deleting-destructor entry, not the complete-destructor entry.
    clang::GlobalDecl global_decl;
    if (const auto* destructor = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
        const auto destructor_kind = getASTContext().getTargetInfo().emitVectorDeletingDtors(getASTContext().getLangOpts())
                                         ? clang::Dtor_VectorDeleting
                                         : clang::Dtor_Deleting;
        global_decl                = clang::GlobalDecl(destructor, destructor_kind);
    }
    else {
        global_decl = clang::GlobalDecl(decl);
    }

    const auto location = vtable->getMethodVFTableLocation(global_decl);
    const auto* owner   = decl->getParent();
    auto* dispatch      = p_msg->mutable_virtual_dispatch();
    if (owner->getNumVBases() != 0) {
        auto* complex = dispatch->mutable_complex();
        auto offset   = location.VFPtrOffset;
        setVersioned(complex->mutable_vtable_index(), location.Index);
        // The fixed adjustment applies after any dynamic virtual-base lookup.
        setVersioned(complex->mutable_this_delta(), location.VFPtrOffset.getQuantity());
        if (location.VBase) {
            const auto& layout = getASTContext().getASTRecordLayout(owner);
            setVersioned(complex->mutable_vbptr_offset(), layout.getVBPtrOffset().getQuantity());
            setVersioned(complex->mutable_vbtable_index(), location.VBTableIndex);
            offset += layout.getVBaseClassOffset(location.VBase);
        }
        // Keep the complete-object offset for consumers inspecting a concrete layout.
        setVersioned(complex->mutable_vtable_offset(), offset.getQuantity());
        return;
    }

    // A single direct base may itself inherit from multiple nonvirtual bases.
    const auto* base = owner;
    // Iterate through bases until getNumBases is > 1, meaning we've found a point in the inheritance chain
    // where multiple bases are involved, or getNumBases == 0, meaning we've gone through the entire inheritance
    // chain and didn't find an instance of multiple bases. We need to know this to know whether to construct
    // a SimpleDispatch or MultipleDispatch with VFPtr potentially being zero
    while (base->getNumBases() == 1) {
        base = base->bases_begin()->getType()->getAsCXXRecordDecl();
    }
    if (base->getNumBases() > 1) {
        auto* multiple = dispatch->mutable_multiple();
        setVersioned(multiple->mutable_vtable_index(), location.Index);
        // This offset both locates the vfptr and adjusts this before calling its slot.
        setVersioned(multiple->mutable_vtable_offset(), location.VFPtrOffset.getQuantity());
    }
    else {
        setVersioned(dispatch->mutable_simple()->mutable_vtable_index(), location.Index);
    }
}
