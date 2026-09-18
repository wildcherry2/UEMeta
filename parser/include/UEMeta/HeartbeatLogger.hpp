#pragma once
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace UEMeta {
    class HeartbeatLogger {
    public:
        explicit HeartbeatLogger(std::string str, std::chrono::milliseconds wait = std::chrono::milliseconds(1500));
        void         start();
        virtual void setStr(std::string str);
        void         stop();
        virtual ~HeartbeatLogger() { stop(); }

    protected:
        virtual void       log();
        const std::string& getStr();

    private:
        std::jthread              thread;
        std::string               str;
        std::chrono::milliseconds wait;
    };

    class CountingHeartbeatLogger : public HeartbeatLogger {
    public:
        explicit CountingHeartbeatLogger(const std::string& format_str, std::chrono::milliseconds wait = std::chrono::milliseconds(1500));
        ~CountingHeartbeatLogger() override { stop(); }
        void     setStr(std::string str) override;
        void     increment();
        void     decrement();
        void     setValue(uint64_t value);
        uint64_t getValue() const;

    protected:
        void log() override;

    private:
        std::atomic_uint64_t counter{0};
    };
} // namespace UEMeta
