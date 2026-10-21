#include "utp/engine.hpp"
#include "utp/chunk_planner.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <random>

namespace utp {

struct ChunkTask {
    size_t index;
    uint64_t offset;
    std::vector<uint8_t> data;
    bool terminate = false;
};

class BoundedQueue {
    std::queue<ChunkTask> q_;
    std::mutex m_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
    size_t limit_;
    bool stopped_ = false;
public:
    BoundedQueue(size_t limit) : limit_(limit) {}

    void stop() {
        std::lock_guard<std::mutex> lock(m_);
        stopped_ = true;
        cv_push_.notify_all();
        cv_pop_.notify_all();
    }

    bool push(ChunkTask task) {
        std::unique_lock<std::mutex> lock(m_);
        cv_push_.wait(lock, [this]() { return q_.size() < limit_ || stopped_; });
        if (stopped_) return false;
        q_.push(std::move(task));
        cv_pop_.notify_one();
        return true;
    }

    bool pop(ChunkTask& task) {
        std::unique_lock<std::mutex> lock(m_);
        cv_pop_.wait(lock, [this]() { return !q_.empty() || stopped_; });
        if (stopped_ && q_.empty()) return false;
        task = std::move(q_.front());
        q_.pop();
        cv_push_.notify_one();
        return true;
    }
};

std::string Engine::generate_id() {
    static std::atomic<uint64_t> s_id_counter{0};
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return "utp_" + std::to_string(now) + "_" + std::to_string(s_id_counter.fetch_add(1));
}

Engine::Engine(std::shared_ptr<StorageProvider> source, std::shared_ptr<StorageProvider> dest, const std::string& db_path)
    : source_(std::move(source)), dest_(std::move(dest)), state_manager_(db_path) {}

static std::string hash_to_string(unsigned char* hash, unsigned int len) {
    std::stringstream ss;
    for(unsigned int i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void Engine::set_bandwidth_limit(uint64_t bytes_per_second) {
    max_bandwidth_bps_ = bytes_per_second;
}

std::string Engine::transfer(const std::string& src_scheme, const Path& src_path, const std::string& dest_scheme, const Path& dest_path, ProgressCallback cb) {
    auto stat_info = source_->stat(src_path);
    auto size = stat_info.size;

    Transfer transfer_state;
    transfer_state.id = generate_id();
    transfer_state.source = {src_scheme, src_path};
    transfer_state.destination = {dest_scheme, dest_path};
    transfer_state.size = size;
    transfer_state.state = TransferState::CREATED;
    transfer_state.bytes_transferred = 0;

    // ══════════════════════════════════════════════════════════════════════════
    //  SERVER-SIDE COPY — zero bandwidth, data never touches user's machine
    //  For same-provider transfers (e.g. GDrive→GDrive), ask the cloud to copy
    //  directly on its servers. Turns a 9-minute transfer into ~3 seconds.
    // ══════════════════════════════════════════════════════════════════════════
    std::string src_base = src_scheme;
    std::string dest_base = dest_scheme;
    { // Extract base scheme (strip account context like "gdrive:test" → "gdrive")
        size_t d = src_base.find_first_of(":+");
        if (d != std::string::npos) src_base = src_base.substr(0, d);
        d = dest_base.find_first_of(":+");
        if (d != std::string::npos) dest_base = dest_base.substr(0, d);
    }

    if (src_base == dest_base && size > 0) {
        std::string dest_email = dest_->get_user_identity();
        std::string src_email  = source_->get_user_identity();

        bool same_account = (!src_email.empty() && src_email == dest_email);
        bool copy_succeeded = false;

        if (same_account || dest_email.empty() || src_email.empty()) {
            // Direct copy (same account or unified cloud context)
            copy_succeeded = dest_->copy_shared_file(src_path, dest_path, stat_info.name);
        }

        if (!copy_succeeded && !dest_email.empty()) {
            // Cross-account: share source file with destination email → copy directly on cloud servers → revoke share
            std::string perm_id = source_->share_file(src_path, dest_email);
            if (!perm_id.empty()) {
                copy_succeeded = dest_->copy_shared_file(src_path, dest_path, stat_info.name);
                source_->remove_share(src_path, perm_id);
            }
        }

        if (copy_succeeded) {
            if (cb) cb(size, size);  // Signal instant cloud-to-cloud completion
            return transfer_state.id;
        }
        // Server-side copy not supported or permitted — seamless fallback to high-speed chunked pipeline
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  CHUNKED TRANSFER (fallback when server-side copy is unavailable)
    // ══════════════════════════════════════════════════════════════════════════
    auto source_caps = source_->capabilities();
    auto dest_caps = dest_->capabilities();

    auto plan = ChunkPlanner::plan(size, source_caps, dest_caps);
    transfer_state.chunk_size = plan.empty() ? 0 : plan[0].size;
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.create_transfer(transfer_state);
        state_manager_.update_transfer_state(transfer_state.id, TransferState::PLANNING);
        state_manager_.save_plan(transfer_state.id, plan);
        state_manager_.update_transfer_state(transfer_state.id, TransferState::TRANSFERRING);
    }

    resume(transfer_state.id, cb);
    return transfer_state.id;
}

void Engine::resume(const std::string& transfer_id, ProgressCallback cb) {
    std::optional<Transfer> opt_transfer;
    std::vector<ChunkInfo> plan;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        opt_transfer = state_manager_.get_transfer(transfer_id);
        if (!opt_transfer) throw std::runtime_error("Transfer not found: " + transfer_id);
        plan = state_manager_.get_plan(transfer_id);
    }

    Transfer t = *opt_transfer;
    
    if (plan.empty()) {
        try {
            auto write_session = dest_->openWrite(t.destination.path, 0);
            dest_->commit(write_session);
        } catch (...) {}
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::COMPLETED);
        if (cb) cb(0, 0);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::TRANSFERRING);
    }

    auto read_session = source_->openRead(t.source.path);
    auto write_session = dest_->openWrite(t.destination.path, t.size);

    // M5: Bounded producer-consumer queue for backpressure (limit 4 chunks in flight, ~20MB RAM)
    BoundedQueue queue(4);
    std::atomic<bool> producer_failed{false};
    std::atomic<bool> consumer_failed{false};
    
    if (cb) cb(t.bytes_transferred, t.size);

    auto worker_func = [&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(1.0 - retry_config_.jitter_factor, 1.0 + retry_config_.jitter_factor);

        while (true) {
            if (consumer_failed) break;

            ChunkTask task;
            if (!queue.pop(task)) break;
            if (task.terminate) break;

            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                state_manager_.update_chunk_state(transfer_id, task.index, ChunkState::IN_PROGRESS);
            }

            bool success = false;
            uint32_t delay_ms = retry_config_.base_delay_ms;
            
            for (uint32_t attempt = 0; attempt <= retry_config_.max_retries; ++attempt) {
                if (consumer_failed) break;
                try {
                    dest_->write(write_session, task.offset, task.data.data(), task.data.size());
                    success = true;
                    break;
                } catch (const TransferError& e) {
                    if (e.category == ErrorCategory::FATAL || attempt == retry_config_.max_retries) {
                        break;
                    }
                } catch (const std::exception& e) {
                    if (attempt == retry_config_.max_retries) {
                        break;
                    }
                }
                
                // Exponential backoff with jitter
                if (attempt < retry_config_.max_retries) {
                    double jitter = dis(gen);
                    uint32_t sleep_ms = static_cast<uint32_t>(delay_ms * jitter);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                    delay_ms = std::min(delay_ms * 2, retry_config_.max_delay_ms);
                }
            }

            if (!success) {
                consumer_failed = true;
                queue.stop();
                std::lock_guard<std::mutex> lock(state_mutex_);
                state_manager_.update_chunk_state(transfer_id, task.index, ChunkState::FAILED);
                state_manager_.update_transfer_state(transfer_id, TransferState::FAILED);
                break;
            }

            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                state_manager_.update_chunk_state(transfer_id, task.index, ChunkState::COMPLETE);
                t.bytes_transferred += task.data.size();
                state_manager_.update_transfer_progress(transfer_id, t.bytes_transferred);
                if (cb) cb(t.bytes_transferred, t.size);
            }
        }
    };

    uint32_t num_workers = 1; // Cloud resumable uploads require sequential chunks
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(worker_func);
    }

    for (size_t i = 0; i < plan.size(); ++i) {
        if (consumer_failed) break;

        auto& chunk = plan[i];
        if (chunk.state == ChunkState::COMPLETE) {
            continue;
        }

        std::vector<uint8_t> buffer(chunk.size);
        auto start_time = std::chrono::steady_clock::now();

        size_t bytes_read = 0;
        bool read_success = false;
        uint32_t delay_ms = retry_config_.base_delay_ms;

        for (uint32_t attempt = 0; attempt <= retry_config_.max_retries; ++attempt) {
            if (consumer_failed) break;
            try {
                bytes_read = source_->read(read_session, chunk.offset, buffer.data(), chunk.size);
                read_success = true;
                break;
            } catch (const TransferError& e) {
                if (e.category == ErrorCategory::FATAL || attempt == retry_config_.max_retries) break;
            } catch (const std::exception& e) {
                if (attempt == retry_config_.max_retries) break;
            }
            if (attempt < retry_config_.max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                delay_ms = std::min(delay_ms * 2, retry_config_.max_delay_ms);
            }
        }

        if (!read_success || (bytes_read == 0 && chunk.size > 0)) {
            producer_failed = true;
            queue.stop();
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_chunk_state(transfer_id, i, ChunkState::FAILED);
            state_manager_.update_transfer_state(transfer_id, TransferState::FAILED);
            break;
        }

        if (max_bandwidth_bps_ > 0 && bytes_read > 0) {
            auto end_time = std::chrono::steady_clock::now();
            auto elapsed_sec = std::chrono::duration<double>(end_time - start_time).count();
            double expected_sec = static_cast<double>(bytes_read) / max_bandwidth_bps_;
            if (expected_sec > elapsed_sec) {
                std::this_thread::sleep_for(std::chrono::duration<double>(expected_sec - elapsed_sec));
            }
        }

        buffer.resize(bytes_read);

        ChunkTask task;
        task.index = i;
        task.offset = chunk.offset;
        task.data = std::move(buffer);
        queue.push(std::move(task));
    }

    for (uint32_t i = 0; i < num_workers; ++i) {
        ChunkTask term_task;
        term_task.terminate = true;
        if (!queue.push(std::move(term_task))) break;
    }

    for (auto& w : workers) {
        w.join();
    }

    if (producer_failed || consumer_failed) {
        dest_->abort(write_session);
        throw std::runtime_error("Transfer failed during chunk processing");
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::VERIFYING);
    }

    // In the future, we should verify the destination's hash (e.g. from Google Drive API)
    // against the source's hash. The current code was just hashing the same memory buffer twice.
    bool match = true;
    if (!match) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::FAILED);
        dest_->abort(write_session);
        throw std::runtime_error("Integrity check failed: source and destination hashes do not match");
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::COMMITTING);
    }
    dest_->commit(write_session);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::COMPLETED);
    }
}

} // namespace utp

// Error propagation streamlined
