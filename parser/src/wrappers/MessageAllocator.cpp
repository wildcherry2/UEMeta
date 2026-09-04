#include "UEMeta/wrappers/MessageAllocator.hpp"

ParserTypes::TLEnumDeclaration * UEMeta::MessageAllocator::GetEnum() {
    thread_local ParserTypes::TLEnumDeclaration enum_declaration;
    enum_declaration.Clear();
    return &enum_declaration;
}

ParserTypes::TLFileData * UEMeta::MessageAllocator::GetFileData() {
    thread_local ParserTypes::TLFileData file_data;
    file_data.Clear();
    return &file_data;
}

ParserTypes::TLFreeFunctionDeclaration * UEMeta::MessageAllocator::GetFreeFunction() {
    thread_local ParserTypes::TLFreeFunctionDeclaration free_function_declaration;
    free_function_declaration.Clear();
    return &free_function_declaration;
}

ParserTypes::TLRecordDeclaration * UEMeta::MessageAllocator::GetRecord() {
    thread_local ParserTypes::TLRecordDeclaration record;
    record.Clear();
    return &record;
}

ParserTypes::TLGlobalVariableDeclaration * UEMeta::MessageAllocator::GetGlobalVariable() {
    thread_local ParserTypes::TLGlobalVariableDeclaration global_variable_declaration;
    global_variable_declaration.Clear();
    return &global_variable_declaration;
}

