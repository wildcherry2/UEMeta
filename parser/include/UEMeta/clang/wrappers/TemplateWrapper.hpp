#pragma once
#include <TopLevel.pb.h>

#include "boost/hash2/xxh3.hpp"
#include "clang/AST/DeclTemplate.h"
#include "UEMeta/clang/DeclDb.hpp"

namespace UEMeta {
    // Special non-top-level wrapper for filling out TemplateDetails; it does not own any incoming pointers.
    class TemplateWrapper {
    public:
        TemplateWrapper(const clang::TemplateParameterList* declared_params, ParserTypes::TemplateDetails* p_msg, boost::hash2::xxh3_128& hasher,
            const clang::TemplateArgumentList* specialization_args = nullptr, DeclDb::QueryResult primary_template_id = {false});

        void compute();
    private:
        const clang::TemplateParameterList* const declared_params;
        ParserTypes::TemplateDetails* const p_msg;
        boost::hash2::xxh3_128& hasher;
        const clang::TemplateArgumentList* const specialization_args;
        const DeclDb::QueryResult primary_template_id;
    };
}
