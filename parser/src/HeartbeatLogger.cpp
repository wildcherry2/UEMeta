#include "UEMeta/HeartbeatLogger.hpp"
#include <mutex>
#include <condition_variable>
#include <utility>
#include "UEMeta/Cli.hpp"

UEMeta::HeartbeatLogger::HeartbeatLogger(std::string str, const std::chrono::milliseconds wait)
    : str(std::move(str)), wait(wait) {}

void UEMeta::HeartbeatLogger::Start() {
    if (thread.joinable()) return;

    thread = std::jthread([this](std::stop_token stoken) {
        std::mutex mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);

        while (true) {
            const bool stoppedBeforeTimeout = cv.wait_for(lock, stoken, this->wait, [&stoken] {
                return stoken.stop_requested();
            });
            if (stoppedBeforeTimeout) break;
            Log();
        }
    });
}

void UEMeta::HeartbeatLogger::SetStr(std::string str) {
    const bool started = thread.joinable();
    Stop();
    this->str = std::move(str);
    if (started) Start();
}

void UEMeta::HeartbeatLogger::Stop() {
    if (thread.joinable()) {
        thread.request_stop();
        thread.join();
    }
}

void UEMeta::HeartbeatLogger::Log() {
    UEM_INFO(this->str);
}

const std::string& UEMeta::HeartbeatLogger::Str() {
    return str;
}

const std::string& ValidateFmtString(const std::string& str) {
    auto match = str.find("{}");
    if (match == std::string::npos) {
        throw std::runtime_error("Can't start a counting heartbeat logger without a format string with a single substitution!");
    }

    match = str.find("{}", match + 1);
    if (match != std::string::npos) {
        throw std::runtime_error("Can't start a counting heartbeat logger without a format string with a single substitution!");
    }

    return str;
}

UEMeta::CountingHeartbeatLogger::CountingHeartbeatLogger(const std::string& format_str, const std::chrono::milliseconds wait)
    : HeartbeatLogger(ValidateFmtString(format_str), wait) {
}

void UEMeta::CountingHeartbeatLogger::SetStr(const std::string str) {
    HeartbeatLogger::SetStr(ValidateFmtString(str));
}

void UEMeta::CountingHeartbeatLogger::Increment() {
    counter.fetch_add(1);
}

void UEMeta::CountingHeartbeatLogger::Decrement() {
    counter.fetch_add(-1);
}

void UEMeta::CountingHeartbeatLogger::SetValue(const uint64_t value) {
    counter.store(value);
}

void UEMeta::CountingHeartbeatLogger::Log() {
    UEM_INFO(Str(), counter.load());
}
