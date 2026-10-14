#include "UEMeta/utility/Serializer.hpp"

#include <fstream>

#include "google/protobuf/json/json.h"
#include "google/protobuf/util/json_util.h"

namespace {
    constexpr google::protobuf::json::PrintOptions json_options{
        .add_whitespace                       = true,
        .always_print_fields_with_no_presence = true
    };
} // namespace

UEMeta::Serializer& UEMeta::Serializer::getSerializer() {
    static Serializer serializer;
    return serializer;
}

void UEMeta::Serializer::awaitPendingSerializations() {
    serialization_pool.wait();
}

void UEMeta::Serializer::saveToString(const google::protobuf::Message* msg, std::string& out) const {
    if (!google::protobuf::util::MessageToJsonString(*msg, &out, json_options).ok()) {
        UEM_ERROR("Failed to convert serialized output to string!");
    }
}

void UEMeta::Serializer::saveToFile(const google::protobuf::Message* msg, const std::shared_ptr<google::protobuf::Arena>& arena,
                                  std::filesystem::path out_file_path) {
    const auto& cfg     = Config::getConfig();
    const bool  is_json = cfg.getFormat() == Config::SerializationFormat::Json;
    auto serialize_fn = [msg, arena, out_file_path = std::move(out_file_path), is_json] {
        const auto    open_mode = (is_json ? std::ios::out : std::ios::binary) | std::ios::trunc;
        std::ofstream out_file(out_file_path, open_mode);
        if (!out_file) {
            throw std::runtime_error("Failed to open file for writing!");
        }

        if (is_json) {
            thread_local std::string buffer;
            buffer.clear();
            if (google::protobuf::util::MessageToJsonString(*msg, &buffer, json_options).ok()) {
                out_file << buffer;
            }
            else {
                UEM_ERROR("Failed to write to file: {}", out_file_path.string());
            }
        }
        else if (!msg->SerializeToOstream(&out_file)) {
            UEM_ERROR("Failed to write to file: {}", out_file_path.string());
        }

        (void)arena; // Keep the message's arena alive until serialization completes.
    };

    if (cfg.syncSerialization()) {
        serialize_fn();
    }
    else {
        serialization_pool.detach_task([serialize_fn = std::move(serialize_fn)] {
            try {
                serialize_fn();
            }
            catch (const std::exception& e) {
                UEM_ERROR("Failed to serialize declaration: {}", e.what());
            }
            catch (...) {
                UEM_ERROR("Failed to serialize declaration with unknown exception!");
            }
        });
    }
}
