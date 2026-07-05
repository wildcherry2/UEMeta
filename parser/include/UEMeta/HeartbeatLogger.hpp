#pragma once
#include <thread>
#include <string>
#include <chrono>
#include <atomic>

namespace UEMeta {
    class HeartbeatLogger {
    public:
        explicit HeartbeatLogger(std::string str, std::chrono::milliseconds wait = std::chrono::milliseconds(1500));
        void Start();
        virtual void SetStr(std::string str);
        void Stop();
        virtual ~HeartbeatLogger() = default;

    protected:
        virtual void Log();
        const std::string& Str();

    private:
        std::jthread thread;
        std::string str;
        std::chrono::milliseconds wait;
    };

    class CountingHeartbeatLogger : public HeartbeatLogger {
    public:
        explicit CountingHeartbeatLogger(const std::string& format_str, std::chrono::milliseconds wait = std::chrono::milliseconds(1500));
        ~CountingHeartbeatLogger() override = default;
        void SetStr(std::string str) override;
        void Increment();
        void Decrement();
        void SetValue(uint64_t value);
        uint64_t GetValue() const;
    protected:
        void Log() override;

    private:
        std::atomic_uint64_t counter{0};
    };
}