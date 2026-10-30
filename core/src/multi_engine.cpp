#include "utp/multi_engine.hpp"
#include "utp/chunk_planner.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace utp {

static std::string hash_to_hex_string(const unsigned char* hash, unsigned int len) {
    std::stringstream ss;
    for (unsigned int i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string MultiEngine::generate_id() {
    static std::atomic<uint64_t> s_id_counter{0};
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return "utp_multi_" + std::to_string(now) + "_" + std::to_string(s_id_counter.fetch_add(1));
}

MultiEngine::MultiEngine(std::shared_ptr<StorageProvider> source, const std::string& db_path)
    : source_(std::move(source)), state_manager_(db_path) {}

void MultiEngine::set_bandwidth_limit(uint64_t bytes_per_second) {
    max_bandwidth_bps_ = bytes_per_second;
}

void MultiEngine::set_event_callback(EventCallback cb) {
    event_cb_ = std::move(cb);
}

void MultiEngine::request_pause() {
    pause_requested_.store(true);
}

void MultiEngine::request_cancel() {
    cancel_requested_.store(true);
}

void MultiEngine::emit(EventType type, const std::string& transfer_id, const std::string& payload) {
    Event ev;
    ev.transfer_id = transfer_id;
    ev.type = type;
    ev.payload = payload;
    ev.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        try {
            state_manager_.log_event(transfer_id, type, payload);
        } catch (...) {}
    }
    if (event_cb_) {
        try { event_cb_(ev); } catch (...) {}
    }
}

MultiTransferResult MultiEngine::transfer(
    const std::string& src_scheme,
    const Path& src_path,
    const std::vector<DestinationTarget>& targets,
    MultiProgressCallback cb,
    MultiTransferPolicy policy,
    const std::string& existing_id)
{
    pause_requested_.store(false);
    cancel_requested_.store(false);

    auto start_time = std::chrono::steady_clock::now();
    std::string multi_id = existing_id.empty() ? generate_id() : existing_id;

    MultiTransferResult result;
    result.multi_id = multi_id;

    if (targets.empty()) {
        throw std::invalid_argument("MultiEngine: At least one destination target is required");
    }
    if (!source_) {
        throw std::invalid_argument("MultiEngine: Source provider is null");
    }

    // 1. STAT SOURCE
    FileInfo src_info = source_->stat(src_path);
    if (src_info.is_directory) {
        throw std::runtime_error("MultiEngine currently operates on files; directory fan-out is handled per file");
    }
    uint64_t total_size = src_info.size;
    result.total_source_size = total_size;

    // 2. INITIALIZE IN SQLITE LEDGER
    state_manager_.create_multi_transfer(multi_id, src_scheme + "://" + src_path, policy, targets, total_size);
    emit(EventType::MULTI_TRANSFER_STARTED, multi_id,
         std::to_string(targets.size()) + " destinations · " + std::to_string(total_size) + " bytes · policy: " + multi_transfer_policy_to_string(policy));

    struct TargetRuntime {
        size_t index{0};
        std::string scheme;
        Path path;
        std::string display_name;
        std::shared_ptr<StorageProvider> provider;
        WriteSession session;
        bool has_session{false};
        bool active{true};
        bool completed{false};
        bool failed{false};
        uint64_t bytes_written{0};
        std::string error;
        std::string sha256;
        EVP_MD_CTX* sha_ctx{nullptr};
    };

    std::vector<TargetRuntime> runtimes(targets.size());
    for (size_t i = 0; i < targets.size(); ++i) {
        runtimes[i].index = i;
        runtimes[i].scheme = targets[i].scheme;
        runtimes[i].path = targets[i].path;
        runtimes[i].display_name = targets[i].display_name.empty() ? (targets[i].scheme + "://" + targets[i].path) : targets[i].display_name;
        runtimes[i].provider = targets[i].provider;
    }

    // Auto-cleanup helper for EVP contexts and uncommitted write sessions
    auto cleanup_runtimes = [&](bool abort_all) {
        for (auto& rt : runtimes) {
            if (rt.sha_ctx) {
                EVP_MD_CTX_free(rt.sha_ctx);
                rt.sha_ctx = nullptr;
            }
            if (rt.has_session && (abort_all || !rt.completed)) {
                try { rt.provider->abort(rt.session); } catch (...) {}
                rt.has_session = false;
            }
        }
    };

    // 3. CAPABILITY NEGOTIATION & WRITE SESSION INITIALIZATION
    ProviderCapabilities src_caps = source_->capabilities();
    ProviderCapabilities min_dest_caps = runtimes[0].provider->capabilities();

    for (size_t i = 0; i < runtimes.size(); ++i) {
        auto& rt = runtimes[i];
        try {
            auto caps = rt.provider->capabilities();
            if (caps.max_chunk_size < min_dest_caps.max_chunk_size) {
                min_dest_caps.max_chunk_size = caps.max_chunk_size;
            }
            rt.session = rt.provider->openWrite(rt.path, total_size, "");
            rt.has_session = true;
            rt.sha_ctx = EVP_MD_CTX_new();
            if (!rt.sha_ctx || EVP_DigestInit_ex(rt.sha_ctx, EVP_sha256(), nullptr) != 1) {
                throw std::runtime_error("Failed to initialize OpenSSL SHA-256 context");
            }
        } catch (const std::exception& e) {
            rt.active = false;
            rt.failed = true;
            rt.error = e.what();
            result.errors.push_back("Destination [" + rt.display_name + "] failed to open: " + e.what());
            state_manager_.update_multi_dest_progress(multi_id, i, 0, "FAILED", e.what());
            emit(EventType::MULTI_DEST_FAILED, multi_id, rt.display_name + ": " + e.what());

            if (policy == MultiTransferPolicy::ALL_OR_NOTHING) {
                cleanup_runtimes(true);
                state_manager_.complete_multi_transfer(multi_id, false);
                emit(EventType::MULTI_TRANSFER_FAILED, multi_id, "Aborted on open failure: " + std::string(e.what()));
                result.overall_success = false;
                for (const auto& r : runtimes) {
                    DestinationResult dr;
                    dr.target_index = r.index;
                    dr.scheme = r.scheme;
                    dr.path = r.path;
                    dr.display_name = r.display_name;
                    dr.success = false;
                    dr.error = r.error;
                    result.destinations.push_back(dr);
                }
                return result;
            }
        }
    }

    // 4. CHUNK PLANNING
    auto plan = ChunkPlanner::plan(total_size, src_caps, min_dest_caps);

    auto update_progress_cb = [&]() {
        if (!cb) return;
        std::vector<DestinationProgress> p_list;
        for (const auto& rt : runtimes) {
            DestinationProgress dp;
            dp.target_index = rt.index;
            dp.display_name = rt.display_name;
            dp.scheme = rt.scheme;
            dp.path = rt.path;
            dp.bytes_transferred = rt.bytes_written;
            dp.total_bytes = total_size;
            dp.active = rt.active;
            dp.completed = rt.completed;
            dp.failed = rt.failed;
            dp.error = rt.error;
            p_list.push_back(dp);
        }
        cb(p_list);
    };

    update_progress_cb();

    // 5. ZERO-BYTE FILE OPTIMIZATION
    if (total_size == 0) {
        for (size_t i = 0; i < runtimes.size(); ++i) {
            auto& rt = runtimes[i];
            if (!rt.active) continue;
            try {
                rt.provider->commit(rt.session);
                rt.has_session = false;
                rt.completed = true;
                rt.sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"; // Empty SHA-256
                state_manager_.update_multi_dest_progress(multi_id, i, 0, "COMPLETED", "", rt.sha256);
                emit(EventType::MULTI_DEST_COMPLETED, multi_id, rt.display_name);
            } catch (const std::exception& e) {
                rt.active = false;
                rt.failed = true;
                rt.error = e.what();
                state_manager_.update_multi_dest_progress(multi_id, i, 0, "FAILED", e.what());
                emit(EventType::MULTI_DEST_FAILED, multi_id, rt.display_name + ": " + e.what());
            }
        }
    } else {
        // 6. ZERO-DISK BOUNDED MEMORY FAN-OUT STREAMING
        auto read_session = source_->openRead(src_path);

        for (size_t chunk_idx = 0; chunk_idx < plan.size(); ++chunk_idx) {
            if (cancel_requested_.load()) {
                cleanup_runtimes(true);
                state_manager_.complete_multi_transfer(multi_id, false);
                emit(EventType::TRANSFER_CANCELLED, multi_id, "User requested cancel");
                throw std::runtime_error("Transfer " + multi_id + " cancelled by user");
            }
            if (pause_requested_.load()) {
                cleanup_runtimes(true);
                emit(EventType::TRANSFER_PAUSED, multi_id, "User requested pause");
                throw std::runtime_error("Transfer " + multi_id + " paused by user");
            }

            const auto& chunk = plan[chunk_idx];
            std::vector<uint8_t> chunk_buf(chunk.size);

            // Read single chunk from source into RAM buffer
            size_t bytes_read = source_->read(read_session, chunk.offset, chunk_buf.data(), chunk.size);
            if (bytes_read == 0 && chunk.size > 0) {
                throw std::runtime_error("Premature EOF encountered reading source at offset " + std::to_string(chunk.offset));
            }

            // Fan-out chunk concurrently to all active destinations
            std::vector<std::future<void>> write_futures;
            for (size_t i = 0; i < runtimes.size(); ++i) {
                if (!runtimes[i].active) continue;
                write_futures.push_back(std::async(std::launch::async, [&, i, offset = chunk.offset, size = bytes_read]() {
                    auto& rt = runtimes[i];
                    try {
                        rt.provider->write(rt.session, offset, chunk_buf.data(), size);
                        if (rt.sha_ctx) {
                            EVP_DigestUpdate(rt.sha_ctx, chunk_buf.data(), size);
                        }
                        rt.bytes_written += size;
                    } catch (const std::exception& e) {
                        rt.active = false;
                        rt.failed = true;
                        rt.error = e.what();
                    }
                }));
            }

            for (auto& f : write_futures) {
                f.get();
            }

            // Evaluate failures & policy enforcement
            bool any_failed = false;
            size_t active_count = 0;
            for (size_t i = 0; i < runtimes.size(); ++i) {
                auto& rt = runtimes[i];
                if (rt.failed && rt.has_session) {
                    any_failed = true;
                    try { rt.provider->abort(rt.session); } catch (...) {}
                    rt.has_session = false;
                    state_manager_.update_multi_dest_progress(multi_id, i, rt.bytes_written, "FAILED", rt.error);
                    emit(EventType::MULTI_DEST_FAILED, multi_id, rt.display_name + ": " + rt.error);
                    result.errors.push_back("Destination [" + rt.display_name + "] failed: " + rt.error);
                }
                if (rt.active) {
                    active_count++;
                    state_manager_.update_multi_dest_progress(multi_id, i, rt.bytes_written, "TRANSFERRING");
                }
            }

            if (any_failed && policy == MultiTransferPolicy::ALL_OR_NOTHING) {
                cleanup_runtimes(true);
                state_manager_.complete_multi_transfer(multi_id, false);
                emit(EventType::MULTI_TRANSFER_FAILED, multi_id, "Aborted: policy ALL_OR_NOTHING detected failure");
                result.overall_success = false;
                for (const auto& r : runtimes) {
                    DestinationResult dr;
                    dr.target_index = r.index;
                    dr.scheme = r.scheme;
                    dr.path = r.path;
                    dr.display_name = r.display_name;
                    dr.success = false;
                    dr.bytes_transferred = r.bytes_written;
                    dr.error = r.error.empty() ? "Aborted due to peer destination failure" : r.error;
                    result.destinations.push_back(dr);
                }
                return result;
            }

            if (active_count == 0) {
                break; // All destinations failed in BEST_EFFORT
            }

            update_progress_cb();
            emit(EventType::MULTI_DEST_CHUNK_COMPLETED, multi_id,
                 "Chunk " + std::to_string(chunk_idx + 1) + "/" + std::to_string(plan.size()));

            // Bandwidth throttling
            if (max_bandwidth_bps_ > 0) {
                uint64_t chunk_bytes = bytes_read;
                double expected_secs = static_cast<double>(chunk_bytes) / static_cast<double>(max_bandwidth_bps_);
                uint32_t sleep_ms = static_cast<uint32_t>(expected_secs * 1000.0);
                if (sleep_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
            }
        }
    }

    // 7. COMMIT & VERIFY ALL ACTIVE DESTINATIONS
    auto end_time = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(end_time - start_time).count();
    result.total_duration_seconds = total_sec;

    size_t successful_targets = 0;
    for (size_t i = 0; i < runtimes.size(); ++i) {
        auto& rt = runtimes[i];
        DestinationResult dr;
        dr.target_index = rt.index;
        dr.scheme = rt.scheme;
        dr.path = rt.path;
        dr.display_name = rt.display_name;
        dr.bytes_transferred = rt.bytes_written;
        dr.duration_seconds = total_sec;

        if (rt.active && !rt.failed) {
            try {
                // Finalize cryptographic SHA-256 hash
                if (rt.sha_ctx) {
                    unsigned char digest[EVP_MAX_MD_SIZE];
                    unsigned int dlen = 0;
                    EVP_DigestFinal_ex(rt.sha_ctx, digest, &dlen);
                    EVP_MD_CTX_free(rt.sha_ctx);
                    rt.sha_ctx = nullptr;
                    rt.sha256 = hash_to_hex_string(digest, dlen);
                }

                rt.provider->commit(rt.session);
                rt.has_session = false;
                rt.completed = true;
                dr.success = true;
                dr.sha256 = rt.sha256;
                successful_targets++;

                state_manager_.update_multi_dest_progress(multi_id, i, rt.bytes_written, "COMPLETED", "", rt.sha256);
                emit(EventType::MULTI_DEST_COMPLETED, multi_id, rt.display_name + " (" + rt.sha256 + ")");
            } catch (const std::exception& e) {
                rt.active = false;
                rt.failed = true;
                rt.error = e.what();
                dr.success = false;
                dr.error = e.what();
                state_manager_.update_multi_dest_progress(multi_id, i, rt.bytes_written, "FAILED", e.what());
                emit(EventType::MULTI_DEST_FAILED, multi_id, rt.display_name + ": " + e.what());
            }
        } else {
            dr.success = false;
            dr.error = rt.error;
        }

        result.destinations.push_back(dr);
    }

    cleanup_runtimes(false);
    update_progress_cb();

    // 8. FINAL OVERALL RESULT
    if (policy == MultiTransferPolicy::ALL_OR_NOTHING) {
        result.overall_success = (successful_targets == runtimes.size());
    } else {
        result.overall_success = (successful_targets > 0);
    }

    state_manager_.complete_multi_transfer(multi_id, result.overall_success);
    if (result.overall_success) {
        emit(EventType::MULTI_TRANSFER_COMPLETED, multi_id,
             std::to_string(successful_targets) + "/" + std::to_string(runtimes.size()) + " destinations succeeded in " + std::to_string(total_sec) + "s");
    } else {
        emit(EventType::MULTI_TRANSFER_FAILED, multi_id, "Multi-destination transfer failed requirements");
    }

    return result;
}

} // namespace utp
