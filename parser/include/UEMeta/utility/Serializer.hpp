#pragma once

#include <filesystem>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "BS_thread_pool.hpp"
#include "UEMeta/clang/DeclUtility.hpp"

namespace UEMeta {
    class Serializer final {
    public:
        static Serializer& getSerializer();

        Serializer(const Serializer&) = delete;
        Serializer(Serializer&&) = delete;
        Serializer& operator=(const Serializer&) = delete;
        Serializer& operator=(Serializer&&) = delete;

        // Waits for queued and running tasks; call from the thread reading the AST.
        // Detached task failures are logged by the tasks themselves.
        void awaitPendingSerializations();

        void saveToString(const google::protobuf::Message* msg, std::string& out) const;

        template <ProtoMessage PT>
            requires (TopLevelProto<PT> || std::same_as<PT, ParserTypes::ForwardDeclarationList> || std::same_as<PT, ParserTypes::ReflectionCache>)
        void saveToFile(const PT* msg, const std::shared_ptr<google::protobuf::Arena>& arena) {
            if (!msg)
                throw std::invalid_argument("Failed to serialize because msg is null!");
            if (!arena)
                throw std::invalid_argument("Failed to serialize because arena is null!");

            const auto&            cfg     = Config::getConfig();
            const bool             is_json = cfg.getFormat() == Config::SerializationFormat::Json;
            const std::string_view type    = is_json ? "json" : "bin";
            const auto&            out_dir = cfg.getOutputDirectory().getUnderlyingPath();
            std::filesystem::path  out_file_path;
            if constexpr (TopLevelProto<PT>) {
                const ParserTypes::DeclarationMetadata& metadata = msg->metadata();
                if (cfg.prefersFullNameInFileName()) {
                    auto name = std::string_view{metadata.qualified_name()} | std::views::split(std::string_view{"::"}) |
                                std::views::join_with(std::string_view{"."}) | std::ranges::to<std::string>();
                    out_file_path = out_dir / fmtquill::format("{}-{}.{}{}", name, metadata.occurrence_index().versions(0).value(),
                                                             TOP_LEVEL_EXT<PT>, type);
                }
                else {
                    out_file_path = out_dir / fmtquill::format("{}{}.{}{}", metadata.decl_id().a(), metadata.decl_id().b(), TOP_LEVEL_EXT<PT>, type);
                }
            }
            else if constexpr (std::same_as<PT, ParserTypes::ForwardDeclarationList>) {
                out_file_path = out_dir / (is_json ? "fwd.decljson" : "fwd.declbin");
            }
            else if constexpr (std::same_as<PT, ParserTypes::ReflectionCache>) {
                out_file_path = out_dir / (is_json ? "cache.reflectionjson" : "cache.reflection");
            }

            saveToFile(msg, arena, std::move(out_file_path));
        }

    private:
        Serializer() = default;
        ~Serializer() = default;

        void saveToFile(const google::protobuf::Message* msg, const std::shared_ptr<google::protobuf::Arena>& arena,
                        std::filesystem::path out_file_path);

        BS::thread_pool<> serialization_pool;
    };
} // namespace UEMeta
