#include "UEMeta/utility/HeartbeatLogger.hpp"
#include <condition_variable>
#include <mutex>
#include <utility>
#include "UEMeta/Cli.hpp"

UEMeta::HeartbeatLogger::HeartbeatLogger(std::string str, const std::chrono::milliseconds wait) : str(std::move(str)), wait(wait) {}

void UEMeta::HeartbeatLogger::start() {
    if (thread.joinable())
        return;

    thread = std::jthread([this](std::stop_token stoken) {
        std::mutex                  mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);

        while (true) {
            log();
            const bool stopped_before_timeout = cv.wait_for(lock, stoken, this->wait, [&stoken] { return stoken.stop_requested(); });
            if (stopped_before_timeout)
                break;
        }
    });
}

void UEMeta::HeartbeatLogger::setStr(std::string str) {
    const bool started = thread.joinable();
    stop();
    this->str = std::move(str);
    if (started)
        start();
}

void UEMeta::HeartbeatLogger::stop() {
    if (thread.joinable()) {
        thread.request_stop();
        thread.join();
    }
}

void UEMeta::HeartbeatLogger::log() { UEM_INFO(this->str); }

const std::string& UEMeta::HeartbeatLogger::getStr() { return str; }

const std::string& validateFmtString(const std::string& str) {
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

UEMeta::CountingHeartbeatLogger::CountingHeartbeatLogger(const std::string& format_str, const std::chrono::milliseconds wait) :
    HeartbeatLogger(validateFmtString(format_str), wait) {}

void UEMeta::CountingHeartbeatLogger::setStr(const std::string str) { HeartbeatLogger::setStr(validateFmtString(str)); }

void UEMeta::CountingHeartbeatLogger::increment() {
    if (counter.load() < 0xffffffffffffffffui64)
        return (void)counter.fetch_add(1);
    UEM_WARN("CountingHeartbeatLogger tried to overflow counter!");
}

void UEMeta::CountingHeartbeatLogger::decrement() {
    if (counter.load() > 0)
        return (void)counter.fetch_add(-1);
    UEM_WARN("CountingHeartbeatLogger tried to underflow counter!");
}

void UEMeta::CountingHeartbeatLogger::setValue(const uint64_t value) { counter.store(value); }

uint64_t UEMeta::CountingHeartbeatLogger::getValue() const { return counter.load(); }

void UEMeta::CountingHeartbeatLogger::log() { UEM_INFO(getStr(), counter.load()); }
