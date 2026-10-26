#pragma once

#include "providers/interface/storage_provider.hpp"
#include "utp/state_manager.hpp"
#include "utp/events.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <mutex>

#include <functional>

namespace utp {

using ProgressCallback = std::function<void(uint64_t bytes_transferred, uint64_t total_size)>;

class Engine {
public:
    Engine(std::shared_ptr<StorageProvider> source, std::shared_ptr<StorageProvider> dest, const std::string& db_path = "utp.db");

    std::string transfer(const std::string& src_scheme, const Path& src_path, const std::string& dest_scheme, const Path& dest_path, ProgressCallback cb = nullptr, const std::string& existing_id = "");
    void resume(const std::string& transfer_id, ProgressCallback cb = nullptr);
    void set_bandwidth_limit(uint64_t bytes_per_second);

    // P2.1: live subscription to the same event stream that hits the ledger.
    void set_event_callback(EventCallback cb);

    // P2.3: in-process control; `utp pause/cancel <id>` uses the ledger
    // control flag, which the engine polls between chunks.
    void request_pause();
    void request_cancel();

    static std::string generate_id();

private:
    std::shared_ptr<StorageProvider> source_;
    std::shared_ptr<StorageProvider> dest_;
    StateManager state_manager_;

    std::mutex state_mutex_; // Protect state_manager_ calls from multiple threads
    RetryConfig retry_config_; // For exponential backoff
    uint64_t max_bandwidth_bps_{0};

    EventCallback event_cb_;
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> cancel_requested_{false};

    void emit(EventType type, const std::string& transfer_id, const std::string& payload = "");
    void resume_impl(const std::string& transfer_id, ProgressCallback cb, bool fresh_transfer);
};


} // namespace utp
