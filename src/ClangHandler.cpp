// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; }
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; }
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; }

std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {

    class Consumer : public clang::ASTConsumer { //todo make free class and add it to a file->consumer map if multiple are made, same for visitor
    public:
        explicit Consumer(ClangHandler* owner) : owner(owner) {}

        void HandleTranslationUnit(clang::ASTContext& ctx) override {
            owner->TraverseDecl(ctx.getTranslationUnitDecl());
        }

    private:
        ClangHandler* owner;
    };

    return std::make_unique<Consumer>(this); //todo log which file the consumer is for
}
