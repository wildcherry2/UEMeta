#pragma once
#include <vector>
#include <simdjson.h>
#include <clang/AST/RecursiveASTVisitor.h>

template <typename builder_type = simdjson::builder::string_builder>
void tag_invoke(simdjson::serialize_tag, builder_type& builder, const clang::EnumConstantDecl*& decl) {
    builder.start_object();
    builder.append_key_value("Identifier", decl->getName());
    builder.append_comma();
    builder.append_key_value("Value", decl->getInitVal().getExtValue());
    builder.end_object();
}

template <typename builder_type = simdjson::builder::string_builder>
void tag_invoke(simdjson::serialize_tag, builder_type& builder, const clang::EnumDecl*& decl) {
    builder.start_object();
    builder.append_key_value("Identifier", decl->getName()); //todo anon handling?
    builder.append_comma();
    builder.append_key_value("BackingType", decl->getIntegerType().getAsString());
    builder.append_comma();
    builder.append_key_value("ScopeTag", decl->isScoped() ? (decl->isScopedUsingClassTag() ? "class" : "struct") : "");
    builder.append_comma();
    builder.append_key_value("Enumerators", std::vector{decl->enumerators()});
    builder.end_object();
}