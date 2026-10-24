#pragma once

#include "providers/interface/storage_provider.hpp"
#include "utp/state_manager.hpp"
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
    static std::string generate_id();

private:
    std::shared_ptr<StorageProvider> source_;
    std::shared_ptr<StorageProvider> dest_;
    StateManager state_manager_;
    
    std::mutex state_mutex_; // Protect state_manager_ calls from multiple threads
    RetryConfig retry_config_; // For exponential backoff
    uint64_t max_bandwidth_bps_{0};
};


} // namespace utp
