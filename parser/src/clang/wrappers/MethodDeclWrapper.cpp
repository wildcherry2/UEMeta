#include "UEMeta/clang/wrappers/FunctionDeclWrapper.hpp"

#include "clang/AST/GlobalDecl.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "UEMeta/clang/ReflectionDb.hpp"

ParserTypes::MemberFunction* UEMeta::MethodDeclWrapper::serialize(bool has_known_layout, const Hash& owner_id) const {
    const auto* owner = decl->getParent();
    if (owner->getNumVBases() != 0 || llvm::any_of(owner->bases(), [](const auto& base) { return base.isVirtual(); })) {
        throw DeclException(decl, "Virtual inheritance is not supported.");
    }

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
    auto* vtable = getASTContext().getVTableContext();

    // Select the deleting destructor for both ABIs. Itanium has a separate complete
    // destructor slot immediately before it; vector deleting destructors are Microsoft-only.
    clang::GlobalDecl global_decl;
    if (const auto* destructor = llvm::dyn_cast<clang::CXXDestructorDecl>(decl)) {
        const auto destructor_kind = vtable->isMicrosoft() &&
                                     getASTContext().getTargetInfo().emitVectorDeletingDtors(getASTContext().getLangOpts())
                                         ? clang::Dtor_VectorDeleting
                                         : clang::Dtor_Deleting;
        global_decl                = clang::GlobalDecl(destructor, destructor_kind);
    }
    else {
        global_decl = clang::GlobalDecl(decl);
    }

    uint64_t vtable_index;
    int64_t vtable_offset = 0;
    if (auto* microsoft = llvm::dyn_cast<clang::MicrosoftVTableContext>(vtable)) {
        const auto location = microsoft->getMethodVFTableLocation(global_decl);
        vtable_index        = location.Index;
        vtable_offset       = location.VFPtrOffset.getQuantity();
    }
    else {
        // Methods declared/overridden by the owning record have primary-vtable slots
        // in Itanium, even when overriding a secondary base's method.
        // The index is relative to the address point stored in the owner's vptr.
        // Adjustments inside secondary-table thunks must not be applied here.
        vtable_index = llvm::cast<clang::ItaniumVTableContext>(vtable)->getMethodVTableIndex(global_decl);
    }
    const auto* owner   = decl->getParent();
    auto* dispatch      = p_msg->mutable_virtual_dispatch();
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
        setVersioned(multiple->mutable_vtable_index(), vtable_index);
        // This offset both locates the vfptr and adjusts this before calling its slot.
        setVersioned(multiple->mutable_vtable_offset(), vtable_offset);
    }
    else {
        setVersioned(dispatch->mutable_simple()->mutable_vtable_index(), vtable_index);
    }
}
