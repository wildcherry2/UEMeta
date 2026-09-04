#pragma once
#include "TopLevel.pb.h"
#include "llvm/ADT/StringRef.h"
#include "UEMeta/Cli.hpp"

namespace UEMeta {
    class MessageAllocator {
    public:
        static ParserTypes::TLEnumDeclaration* GetEnum();
        static ParserTypes::TLFileData* GetFileData();
        static ParserTypes::TLFreeFunctionDeclaration* GetFreeFunction();
        static ParserTypes::TLRecordDeclaration* GetRecord();
        static ParserTypes::TLGlobalVariableDeclaration* GetGlobalVariable();
    };

    template<typename T>
    concept Stringish = std::same_as<T, llvm::StringRef> || std::same_as<T, std::string> || std::same_as<T, std::string_view>;

    template<Stringish ValueType>
    void SetVersionedString(ParserTypes::VersionedString* p_msg, const ValueType& value) {
        const std::string& version_str = Config::GetConfig().Version();
        ParserTypes::VersionedString_VersionItem* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        if constexpr(std::same_as<ValueType, std::string>) {
            p_version->set_value(value);
        }
        else if constexpr (std::same_as<ValueType, std::string_view>) {
            p_version->set_value(std::string(value));
        }
        else {
            p_version->set_value(value.str());
        }
    }

    template<typename T>
    concept PrimitiveVersionedIntegral = std::same_as<T, ParserTypes::VersionedUint32>
        || std::same_as<T, ParserTypes::VersionedUint64>
        || std::same_as<T, ParserTypes::VersionedInt64>;

    template<std::integral ValueType, PrimitiveVersionedIntegral MessageType>
    void SetVersionedInteger(MessageType* p_msg, ValueType value) {
        const std::string& version_str = Config::GetConfig().Version();
        auto* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        p_version->set_value(value);
    }

    inline void SetVersionedUint64List(ParserTypes::VersionedUint64List* p_msg, const std::vector<uint64_t>& value_vec) {
        const std::string& version_str = Config::GetConfig().Version();
        auto* p_version = p_msg->add_versions();
        p_version->add_source_versions(version_str);
        for (const uint64_t value : value_vec) {
            p_version->add_value(value);
        }
    }
}
