// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <string_view>
#include <thread>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>

#include "UEMeta/Cli.hpp"

/// @brief Tracks whether any guarded Clang callback caught an exception.
static std::atomic_bool GClangExceptionCaught{false};

/// @brief Records a Clang processing exception and logs the failing step.
static void LogClangException(const std::string_view step, const std::exception& ex) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with exception: {}", step, ex.what());
    } catch (...) {
    }
}

/// @brief Records an unknown Clang processing exception and logs the failing step.
static void LogClangUnknownException(const std::string_view step) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with unknown exception", step);
    } catch (...) {
    }
}

/// @brief Runs a Clang callback with exception logging and failure recording.
template <typename Func>
static void GuardClangCallback(const std::string_view step, Func&& func) noexcept {
    try {
        std::forward<Func>(func)();
    } catch (const std::exception& ex) {
        LogClangException(step, ex);
    } catch (...) {
        LogClangUnknownException(step);
    }
}

/// @brief Requests traversal of template instantiations.
bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; }

/// @brief Skips implicit compiler-generated declarations.
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; }

/// @brief Skips lambda body traversal.
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; }

/// @brief Runs Clang with ClangHandler and converts guarded exceptions into a nonzero result.
int UEMeta::RunClangTool(clang::tooling::ClangTool& tool) noexcept {
    GClangExceptionCaught.store(false, std::memory_order_relaxed);
    try {
        const auto result = tool.run(clang::tooling::newFrontendActionFactory<ClangHandler>().get());
        return GClangExceptionCaught.load(std::memory_order_relaxed) ? 1 : result;
    } catch (const std::exception& ex) {
        LogClangException("ClangTool::run", ex);
    } catch (...) {
        LogClangUnknownException("ClangTool::run");
    }

    return 1;
}

/// @brief Starts declaration traversal progress reporting.
void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext&) {
    GuardClangCallback("BeginTranslationUnit", [] {
        UEM_SPINNER_START("Traversing AST");
    });
}

/// @brief Stops declaration traversal progress reporting.
void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) {
    GuardClangCallback("EndTranslationUnit", [] {
        UEM_SPINNER_STOP("Finished traversing AST");
    });
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl*) { return true; }

bool UEMeta::ClangHandler::VisitClassTemplateDecl(clang::ClassTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl*) { return true; }

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl*) { return true; }

bool UEMeta::ClangHandler::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl*) { return true; }

bool UEMeta::ClangHandler::VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl*) { return true; }

bool UEMeta::ClangHandler::VisitVarTemplateDecl(clang::VarTemplateDecl*) { return true; }

/// @brief Creates the AST consumer for one translation unit.
std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance&,
                                                                            llvm::StringRef file) {
    try {
        const auto tu_name = std::filesystem::path(file.str()).filename().string();
        UEM_SPINNER_START(fmtquill::format("Parsing TU '{}' (this may take a moment)",
                                           tu_name.empty() ? file.str() : tu_name));
        ticker_thread = std::jthread([tu_name](std::stop_token token) {
            GuardClangCallback("translation unit spinner", [&] {
                while (true) {
                    if (token.stop_requested()) {
                        UEM_SPINNER_STOP(fmtquill::format("TU '{}' parsed!", tu_name));
                        return;
                    }
                    UEM_SPINNER_TICK;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        });

        /// @brief AST consumer that drives traversal once Clang finishes parsing a translation unit.
        class Consumer : public clang::ASTConsumer {
        public:
            /// @brief Creates a consumer tied to the owning ClangHandler.
            explicit Consumer(ClangHandler* owner) : owner(owner) {}

            /// @brief Stops the TU spinner and traverses declarations.
            void HandleTranslationUnit(clang::ASTContext& ctx) override {
                GuardClangCallback("HandleTranslationUnit", [&] {
                    if (owner->ticker_thread.joinable()) {
                        owner->ticker_thread.request_stop();
                        owner->ticker_thread.join();
                    }
                    UEM_INFO("Starting AST traversal...");
                    owner->BeginTranslationUnit(ctx);
                    if (!owner->TraverseDecl(ctx.getTranslationUnitDecl())) {
                        UEM_ERROR("(clang) AST traversal aborted due to an earlier exception.");
                        return;
                    }
                    owner->EndTranslationUnit(ctx);
                });
            }

        private:
            ClangHandler* owner;
        };

        return std::make_unique<Consumer>(this);
    } catch (const std::exception& ex) {
        LogClangException("CreateASTConsumer", ex);
    } catch (...) {
        LogClangUnknownException("CreateASTConsumer");
    }

    return std::make_unique<clang::ASTConsumer>();
}
