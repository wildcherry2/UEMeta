#pragma once
#include <algorithm>
#include <execution>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#if defined(DEBUG)
#include "buf/validate/validator.h"
#endif
#include "parser.pb.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "clang/AST/Decl.h"

void PopulateEnumDetails(clang::ASTContext& context, ParseResult::EnumDetails* p_msg, const clang::EnumDecl* decl);
void PopulateTemplateDetails(clang::ASTContext& context, ParseResult::TemplateDetails* p_msg, const clang::Decl* decl);

namespace UEMeta {
    class ASTData {
    public:
        [[nodiscard]] clang::ASTContext& GetContext() const {
            if (!context) throw std::runtime_error("Called GetContext before context was initialized!");
            return *context;
        }

        void SetContext(clang::ASTContext* ctx) {
            context = ctx;
        }

        template<typename MessageType>
        [[nodiscard]] MessageType* Allocate() const {
            MessageType* msg = google::protobuf::Arena::Create<MessageType>(&arena);
            to_serialize.push_back(msg);
            return msg;
        }

        void Invalidate(const google::protobuf::Message* msg) const {
            if (const auto found = std::find(std::execution::par_unseq, to_serialize.begin(), to_serialize.end(), msg); found != to_serialize.end()) {
                to_serialize.erase(found);
            }
        }

        void AddVisitedDecl(const clang::Decl* clang_decl, ParseResult::Declaration* p_decl) {
            visited_decls.insert(std::pair<const clang::Decl*, ParseResult::Declaration*>{clang_decl, p_decl});
            all_unique_visited_decls.push_back(clang_decl);
        }

        [[nodiscard]] bool OnVisit(const auto* decl) {
            if (!decl) return true;

            if (const auto as_tag = llvm::dyn_cast_or_null<clang::TagDecl>(decl)) {
                // if this is a forward declaration...
                if (!as_tag->isThisDeclarationADefinition()) {
                    // and we don't know about it yet...
                    if (!visited_forward_decls.contains(decl)) {
                        // generate a new forward declaration message and return
                        auto& ast_context = GetContext();
                        const auto p_decl = Allocate<ParseResult::Declaration>();
                        auto* p_forward_decl = p_decl->mutable_forward_declaration();

                        // assign the kind and template info
                        switch (as_tag->getTagKind()) {
                            case clang::TagTypeKind::Struct:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_STRUCT);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Union:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_UNION);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Class:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_CLASS);
                                PopulateTemplateDetails(ast_context, p_forward_decl->mutable_template_details(), decl);
                                break;
                            case clang::TagTypeKind::Enum:
                                p_forward_decl->set_kind(ParseResult::FORWARD_DECLARATION_KIND_ENUM);
                                PopulateEnumDetails(ast_context, p_forward_decl->mutable_enum_details(), llvm::dyn_cast_or_null<clang::EnumDecl>(decl));
                                break;
                            default:
                                throw std::runtime_error("AddForwardDeclaration can't be called on a nonstandard declaration!");
                        }

                        visited_forward_decls.insert(decl);
                        all_unique_visited_decls.push_back(decl);
                        return OnAfterVisit(as_tag, p_decl->mutable_forward_declaration()->metadata().identifier().qualified_name_hash());
                    }
                    return true;
                }
            }
            // if this is a complete definition that we've already seen (secondary translation unit), continue
            if (visited_decls.contains(decl)) return true;

            return false;
        }

        [[nodiscard]] bool OnAfterVisit(const clang::Decl* clang_decl, const uint64_t clang_decl_fqn_hash) const {
            // populate parent nested hashes
            if (const auto decl_context = clang_decl->getDeclContext(); decl_context->isRecord()) {
                auto* as_cls = llvm::cast<clang::RecordDecl>(decl_context);
                if (const auto parent = visited_decls.find(as_cls); parent != visited_decls.end()) {
                    parent->getSecond()->mutable_record()->add_nested_hashes(clang_decl_fqn_hash);
                }
            }
            return true;
        }

        const std::vector<google::protobuf::Message*>& GetAllMessages() const {
            return to_serialize;
        }

        #if defined(DEBUG)
        [[nodiscard]] buf::validate::Validator CreateValidator() const {
            static std::unique_ptr<buf::validate::ValidatorFactory> factory = buf::validate::ValidatorFactory::New().value();
            return factory->NewValidator(&arena);
        }
        #endif

    private:
        clang::ASTContext* context{};

        // We use maps to make sure we aren't double visiting, and so we can lookup parent structures when
        // we see that a declaration is nested within another.
        llvm::DenseMap<const clang::Decl*, ParseResult::Declaration*> visited_decls{};
        llvm::DenseSet<const clang::Decl*> visited_forward_decls{};

        mutable google::protobuf::Arena arena{};

        // Equivalent to visited_decls.values() + visited_forward_decls.values() + any TLFileData we make.
        // We use a vector that contains the same data as the maps because we want to parallelize serialization.
        // MSVC vectorizes maps (walks through each node in the map and adds them to a vector) which is an
        // immediate O(N) cost when using parallel std::foreach, while other implementations fall back to non-parallel
        // behavior when a container without random access is used. Rather than dealing with an O(N) cost or silent
        // non-parallization during serialization, we just keep a vector with the same data as we allocate from the
        // arena.
        mutable std::vector<google::protobuf::Message*> to_serialize{};

        // Equivalent to visited_decls.keys() + visited_forward_decls.keys().
        // We keep this vector so that when we generate TLFileData, we can use sorting with random access. This is
        // a more direct tradeoff of performance > memory.
        mutable std::vector<const clang::Decl*> all_unique_visited_decls{};
    };
}
