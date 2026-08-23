#pragma once

#include <utility>

#include "TopLevel.pb.h"
#include "UEMeta/Cli.hpp"

namespace UEMeta::Proto {
    template <typename VersionedMessage>
    auto* MutableVersionItem(VersionedMessage* message) {
        if (message->versions_size() == 0) {
            auto* item = message->add_versions();
            item->add_source_versions(Config::GetConfig().Version());
            return item;
        }
        return message->mutable_versions(0);
    }

    template <typename VersionedMessage, typename Value>
    void SetVersioned(VersionedMessage* message, Value&& value) {
        MutableVersionItem(message)->set_value(std::forward<Value>(value));
    }

    inline void SetVersioned(ParserTypes::VersionedBool* message, const bool value) {
        if (value) {
            message->add_true_versions(Config::GetConfig().Version());
        }
        else {
            message->add_false_versions(Config::GetConfig().Version());
        }
    }

    template <typename VersionedMessage, typename Value>
    void AddVersioned(VersionedMessage* message, Value&& value) {
        MutableVersionItem(message)->add_value(std::forward<Value>(value));
    }

    template <typename VersionedMessage>
    decltype(auto) GetVersioned(const VersionedMessage& message) {
        return message.versions(0).value();
    }

    inline bool GetVersioned(const ParserTypes::VersionedBool& message) {
        return !message.true_versions().empty();
    }
}
