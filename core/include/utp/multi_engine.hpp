#pragma once

#include "providers/interface/storage_provider.hpp"
#include "utp/state_manager.hpp"
#include "utp/events.hpp"
#include "utp/multi_types.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace utp {

class MultiEngine {
public:
    explicit MultiEngine(std::shared_ptr<StorageProvider> source, const std::string& db_path = "utp.db");

    MultiTransferResult transfer(
        const std::string& src_scheme,
        const Path& src_path,
        const std::vector<DestinationTarget>& targets,
        MultiProgressCallback cb = nullptr,
        MultiTransferPolicy policy = MultiTransferPolicy::ALL_OR_NOTHING,
        const std::string& existing_id = ""
    );

    void set_bandwidth_limit(uint64_t bytes_per_second);
    void set_event_callback(EventCallback cb);
    void request_pause();
    void request_cancel();

    static std::string generate_id();

private:
    std::shared_ptr<StorageProvider> source_;
    StateManager state_manager_;
    std::mutex state_mutex_;
    RetryConfig retry_config_;
    uint64_t max_bandwidth_bps_{0};

    EventCallback event_cb_;
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> cancel_requested_{false};

    void emit(EventType type, const std::string& transfer_id, const std::string& payload = "");
};

} // namespace utp
