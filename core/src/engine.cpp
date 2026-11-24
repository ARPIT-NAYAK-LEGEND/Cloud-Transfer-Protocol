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
    size_t bytes_ = 0;
    size_t byte_budget_;
    bool stopped_ = false;
public:
    // P1.5: bounded by BYTES, not task count. With chunk sizes scaling to
    // 40MB (and up to 150MB on Dropbox), a task-count bound allowed up to
    // ~240MB of chunk buffers in flight — far beyond the stated memory budget.
    explicit BoundedQueue(size_t byte_budget) : byte_budget_(byte_budget) {}

    void stop() {
        std::lock_guard<std::mutex> lock(m_);
        stopped_ = true;
        cv_push_.notify_all();
        cv_pop_.notify_all();
    }

    bool push(ChunkTask task) {
        std::unique_lock<std::mutex> lock(m_);
        cv_push_.wait(lock, [this, &task]() { return bytes_ + task.data.size() <= byte_budget_ || stopped_; });
        if (stopped_) return false;
        bytes_ += task.data.size();
        q_.push(std::move(task));
        cv_pop_.notify_one();
        return true;
    }

    bool pop(ChunkTask& task) {
        std::unique_lock<std::mutex> lock(m_);
        cv_pop_.wait(lock, [this]() { return !q_.empty() || stopped_; });
        if (stopped_ && q_.empty()) return false;
        task = std::move(q_.front());
        bytes_ -= task.data.size();
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

// Stream a provider object back through a bounded buffer and compute its SHA-256.
// Never loads the whole file into memory (§10 memory discipline).
static std::string compute_provider_sha256(StorageProvider& provider, const Path& path, uint64_t size) {
    auto session = provider.openRead(path);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    const size_t buf_size = 8ULL * 1024 * 1024; // 8 MiB
    std::vector<uint8_t> buf(buf_size);
    uint64_t remaining = size;
    while (remaining > 0) {
        size_t want = static_cast<size_t>(std::min<uint64_t>(remaining, buf_size));
        size_t n = provider.read(session, size - remaining, buf.data(), want);
        if (n == 0) break; // short read / EOF
        EVP_DigestUpdate(ctx, buf.data(), n);
        remaining -= n;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);
    return hash_to_string(digest, digest_len);
}

void Engine::set_bandwidth_limit(uint64_t bytes_per_second) {
    max_bandwidth_bps_ = bytes_per_second;
}

void Engine::set_event_callback(EventCallback cb) {
    event_cb_ = std::move(cb);
}

void Engine::request_pause() { pause_requested_ = true; }
void Engine::request_cancel() { cancel_requested_ = true; }

// P2.1: persist one lifecycle event and forward it to any live subscriber.
// Always call OUTSIDE state_mutex_ scopes — log_event takes its own lock.
void Engine::emit(EventType type, const std::string& transfer_id, const std::string& payload) {
    Event ev;
    ev.transfer_id = transfer_id;
    ev.type = type;
    ev.payload = payload;
    ev.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        try {
            state_manager_.log_event(transfer_id, type, payload);
        } catch (...) {
            // Event persistence must never kill a transfer.
        }
    }
    if (event_cb_) {
        try { event_cb_(ev); } catch (...) {}
    }
}

std::string Engine::transfer(const std::string& src_scheme, const Path& src_path, const std::string& dest_scheme, const Path& dest_path, ProgressCallback cb, const std::string& existing_id) {
    pause_requested_.store(false);
    cancel_requested_.store(false);
    Transfer transfer_state;
    transfer_state.id = existing_id.empty() ? generate_id() : existing_id;
    transfer_state.source = {src_scheme, src_path};
    transfer_state.destination = {dest_scheme, dest_path};
    transfer_state.size = 0; // discovered during DISCOVERING, below
    transfer_state.state = TransferState::CREATED;
    transfer_state.bytes_transferred = 0;

    // P2.2: CREATED is persisted before ANY network call — a crash during
    // auth/stat must still leave a record that the transfer existed (and
    // P1.7 keeps the server-side-copy fast path visible to the ledger).
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.create_transfer(transfer_state);
    }
    emit(EventType::TRANSFER_CREATED,
         transfer_state.id,
         src_scheme + "://" + src_path + " -> " + dest_scheme + "://" + dest_path);

    try {
        // AUTHENTICATING: the provider's identity proves itself on the first
        // API call (tokens were loaded at construction; stat validates them).
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::AUTHENTICATING);
        }

        // DISCOVERING: stat the source.
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::DISCOVERING);
        }
        auto stat_info = source_->stat(src_path);
        auto size = stat_info.size;
        transfer_state.size = size;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_size(transfer_state.id, size);
        }

        // ════════════════════════════════════════════════════════════════════
        //  SERVER-SIDE COPY — zero bandwidth, data never touches user's machine
        //  For same-provider transfers (e.g. GDrive→GDrive), ask the cloud to
        //  copy directly on its servers. Turns a 9-minute transfer into ~3s.
        // ════════════════════════════════════════════════════════════════════
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
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_manager_.update_transfer_progress(transfer_state.id, size);
                    state_manager_.update_transfer_state(transfer_state.id, TransferState::COMPLETED);
                    state_manager_.delete_session_tokens(transfer_state.id);
                }
                emit(EventType::TRANSFER_COMPLETED, transfer_state.id,
                     "server-side copy · " + std::to_string(size) + " bytes");
                if (cb) cb(size, size);  // Signal instant cloud-to-cloud completion
                return transfer_state.id;
            }
            // Server-side copy not supported or permitted — seamless fallback to high-speed chunked pipeline
        }

        // NEGOTIATING: query both providers' capability matrices.
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::NEGOTIATING);
        }
        auto source_caps = source_->capabilities();
        auto dest_caps = dest_->capabilities();

        // PLANNING: build the chunk plan against negotiated limits.
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::PLANNING);
        }
        auto plan = ChunkPlanner::plan(size, source_caps, dest_caps, max_bandwidth_bps_);
        transfer_state.chunk_size = plan.empty() ? 0 : plan[0].size;

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.create_transfer(transfer_state); // upsert fills size/chunk_size
            state_manager_.save_plan(transfer_state.id, plan);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::TRANSFERRING);
        }
    } catch (const std::exception& e) {
        // Anything thrown before the pipeline took over leaves the ledger
        // FAILED with the reason — never parked in AUTHENTICATING/DISCOVERING.
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.record_transfer_error(transfer_state.id, e.what(), 0);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::FAILED);
        }
        emit(EventType::TRANSFER_FAILED, transfer_state.id, e.what());
        throw;
    }

    emit(EventType::TRANSFER_STARTED, transfer_state.id,
         std::to_string(transfer_state.size) + " bytes · chunk_size " + std::to_string(transfer_state.chunk_size));

    // resume_impl owns its own failure bookkeeping.
    resume_impl(transfer_state.id, cb, true);
    return transfer_state.id;
}

void Engine::resume(const std::string& transfer_id, ProgressCallback cb) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto t = state_manager_.get_transfer(transfer_id);
        if (!t) throw std::runtime_error("Transfer not found: " + transfer_id);
        if (t->state == TransferState::CANCELLED) {
            throw std::runtime_error("Transfer " + transfer_id +
                " was cancelled; start a new transfer instead of resuming.");
        }
    }
    emit(EventType::TRANSFER_RESUMED, transfer_id);
    resume_impl(transfer_id, cb, false);
}

void Engine::resume_impl(const std::string& transfer_id, ProgressCallback cb, bool fresh_transfer) {
    (void)fresh_transfer;
    pause_requested_.store(false);
    cancel_requested_.store(false);
    std::optional<Transfer> opt_transfer;
    std::vector<ChunkInfo> plan;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        opt_transfer = state_manager_.get_transfer(transfer_id);
        if (!opt_transfer) throw std::runtime_error("Transfer not found: " + transfer_id);
        plan = state_manager_.get_plan(transfer_id);
        // P2.3: start with a clean control flag — anything older in the ledger
        // must not instantly pause this run.
        state_manager_.clear_control(transfer_id);
    }

    Transfer t = *opt_transfer;

    if (plan.empty()) {
        // P2.5: zero-byte payloads still walk the full lifecycle, and the
        // created object is verified by stat instead of trusting the create
        // call (the old path marked COMPLETED even when commit threw).
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_id, TransferState::COMMITTING);
        }
        auto write_session = dest_->openWrite(t.destination.path, 0, "");
        dest_->commit(write_session);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_id, TransferState::VERIFYING);
        }
        try {
            auto dest_info = dest_->stat(t.destination.path);
            if (dest_info.size != 0) {
                throw std::runtime_error("empty-file verification failed: destination reports " +
                                         std::to_string(dest_info.size) + " bytes");
            }
        } catch (const std::exception& e) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                state_manager_.record_transfer_error(transfer_id, e.what(), 0);
                state_manager_.update_transfer_state(transfer_id, TransferState::FAILED);
            }
            emit(EventType::TRANSFER_FAILED, transfer_id, e.what());
            throw;
        }
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_id, TransferState::COMPLETED);
            state_manager_.delete_session_tokens(transfer_id);
        }
        emit(EventType::TRANSFER_VERIFIED, transfer_id, "empty payload verified");
        emit(EventType::TRANSFER_COMPLETED, transfer_id, "0 bytes");
        if (cb) cb(0, 0);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::TRANSFERRING);
    }

    auto read_session = source_->openRead(t.source.path);

    // P1.3: resume the SAME destination upload session instead of restarting
    // it. The provider's opaque token was persisted on the last openWrite;
    // pass it back so the cloud-side session continues where it left off.
    std::string resume_token;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        resume_token = state_manager_.get_session_token(transfer_id, "dest");
    }
    auto write_session = dest_->openWrite(t.destination.path, t.size, resume_token);
    {
        std::string session_token = dest_->write_session_token(write_session);
        if (!session_token.empty()) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.save_session_token(transfer_id, "dest", session_token);
        }
    }

    // P1.5: bounded by bytes — the budget covers the 64MB floor plus headroom
    // large enough that a single oversized chunk can always be enqueued.
    uint64_t chunk_budget = std::max<uint64_t>(64ULL * 1024 * 1024, 2ULL * t.chunk_size);
    BoundedQueue queue(static_cast<size_t>(chunk_budget));
    std::atomic<bool> producer_failed{false};
    std::atomic<bool> consumer_failed{false};

    // P1.1: mid-transfer AUTH errors trigger at most this many credential
    // refreshes per run — a live refresh loop with a dead token must fail.
    std::atomic<int> auth_refresh_budget{2};
    auto refresh_safely = [](StorageProvider* provider) -> bool {
        try {
            provider->refresh_auth();
            return true;
        } catch (...) {
            return false;
        }
    };

    // P2.3: pause/cancel observed between chunks. The atomics serve embedders
    // (in-process request_pause()/request_cancel()); the ledger flag serves
    // `utp pause/cancel <id>` from another process.
    bool user_paused = false;
    bool user_cancelled = false;

    // Failure forensics — persisted via record_transfer_error (§ errors.md).
    std::mutex err_mutex;
    std::string last_error;
    int last_http = 0;

    if (cb) cb(t.bytes_transferred, t.size);
    emit(EventType::OBJECT_STARTED, transfer_id,
         std::to_string(plan.size()) + " chunks · " + std::to_string(t.size) + " bytes");

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
                    // P2.4: every failed attempt is ledger-forensics.
                    {
                        std::lock_guard<std::mutex> lock(state_mutex_);
                        state_manager_.record_chunk_attempt(transfer_id, task.index);
                    }
                    { std::lock_guard<std::mutex> l(err_mutex); last_error = e.what(); last_http = e.http_status; }
                    // P1.1: a 401 means the token expired mid-transfer; retrying
                    // the same stale token was guaranteed to fail. Refresh once
                    // (bounded), then retry the write immediately.
                    if (e.category == ErrorCategory::AUTH &&
                        auth_refresh_budget.fetch_sub(1) > 0 &&
                        refresh_safely(dest_.get())) {
                        continue;
                    }
                    if (e.category == ErrorCategory::FATAL || attempt == retry_config_.max_retries) {
                        break;
                    }
                } catch (const std::exception& e) {
                    { std::lock_guard<std::mutex> l(err_mutex); last_error = e.what(); last_http = 0; }
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
            }
            if (cb) cb(t.bytes_transferred, t.size);
            emit(EventType::CHUNK_COMPLETED, transfer_id,
                 "chunk " + std::to_string(task.index) + " · offset " + std::to_string(task.offset) +
                 " · " + std::to_string(task.data.size()) + " bytes · " +
                 std::to_string(t.bytes_transferred) + "/" + std::to_string(t.size) + " total");
        }
    };

    uint32_t num_workers = 1; // Cloud resumable uploads require sequential chunks
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(worker_func);
    }

    // Incremental source SHA-256 — updated as we read. On resume, COMPLETE
    // chunks are re-read *only* to keep the digest complete; they are not
    // rewritten (§ verifiability) and never buffered whole (§10).
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> src_ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!src_ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    EVP_DigestInit_ex(src_ctx.get(), EVP_sha256(), nullptr);

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> src_md5_ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!src_md5_ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    EVP_DigestInit_ex(src_md5_ctx.get(), EVP_md5(), nullptr);

    for (size_t i = 0; i < plan.size(); ++i) {
        if (consumer_failed) break;

        // P2.3: control plane between chunks — in-process requests first, then
        // the ledger flag another process may have set via `utp pause/cancel`.
        if (cancel_requested_.load()) user_cancelled = true;
        if (pause_requested_.load())  user_paused = true;
        if (!user_cancelled && !user_paused) {
            std::string control;
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                control = state_manager_.get_control(transfer_id);
            }
            if (control == "cancel") user_cancelled = true;
            else if (control == "pause") user_paused = true;
        }
        if (user_cancelled || user_paused) break;

        auto& chunk = plan[i];
        bool already_complete = (chunk.state == ChunkState::COMPLETE);

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
                // P2.4: every failed attempt is ledger-forensics.
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_manager_.record_chunk_attempt(transfer_id, i);
                }
                { std::lock_guard<std::mutex> l(err_mutex); last_error = e.what(); last_http = e.http_status; }
                // P1.1: refresh the SOURCE credentials on AUTH errors too.
                if (e.category == ErrorCategory::AUTH &&
                    auth_refresh_budget.fetch_sub(1) > 0 &&
                    refresh_safely(source_.get())) {
                    continue;
                }
                if (e.category == ErrorCategory::FATAL || attempt == retry_config_.max_retries) break;
            } catch (const std::exception& e) {
                { std::lock_guard<std::mutex> l(err_mutex); last_error = e.what(); last_http = 0; }
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

        if (bytes_read > 0) {
            EVP_DigestUpdate(src_ctx.get(), buffer.data(), bytes_read);
            EVP_DigestUpdate(src_md5_ctx.get(), buffer.data(), bytes_read);
        }
        // Already-complete chunks were only re-read to finish the source digest.
        if (already_complete) {
            continue;
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

    // A pause/cancel stop wakes the workers immediately; queued chunks drain
    // first so their completed progress is kept.
    if (user_cancelled || user_paused) {
        queue.stop();
    }

    for (uint32_t i = 0; i < num_workers; ++i) {
        ChunkTask term_task;
        term_task.terminate = true;
        if (!queue.push(std::move(term_task))) break;
    }

    for (auto& w : workers) {
        w.join();
    }

    if (user_cancelled) {
        // User asked to abandon: destroy the upload session and temp state.
        dest_->abort(write_session);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_id, TransferState::CANCELLED);
            state_manager_.clear_control(transfer_id);
            state_manager_.delete_session_tokens(transfer_id);
        }
        emit(EventType::TRANSFER_CANCELLED, transfer_id,
             std::to_string(t.bytes_transferred) + "/" + std::to_string(t.size) + " bytes transferred before cancel");
        return;
    }

    if (user_paused) {
        // Keep the temp file + session token: `utp resume <id>` continues
        // exactly here (abort() would delete the local temp file and erase
        // every completed chunk).
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.update_transfer_state(transfer_id, TransferState::PAUSED);
            state_manager_.clear_control(transfer_id);
        }
        emit(EventType::TRANSFER_PAUSED, transfer_id,
             std::to_string(t.bytes_transferred) + "/" + std::to_string(t.size) + " bytes done");
        return;
    }

    if (producer_failed || consumer_failed) {
        // P2.4: failure KEEPS the temp file + session token so `utp resume`
        // continues from the ledger — the old abort() here deleted the local
        // temp file, silently destroying every completed chunk.
        std::string failure_reason;
        int failure_http = 0;
        { std::lock_guard<std::mutex> l(err_mutex); failure_reason = last_error; failure_http = last_http; }
        if (failure_reason.empty()) failure_reason = "Transfer failed during chunk processing";
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.record_transfer_error(transfer_id, failure_reason, failure_http);
        }
        emit(EventType::TRANSFER_FAILED, transfer_id, failure_reason);
        throw std::runtime_error(failure_reason);
    }

    emit(EventType::OBJECT_COMPLETED, transfer_id,
         std::to_string(t.bytes_transferred) + " bytes in " + std::to_string(plan.size()) + " chunks");

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::COMMITTING);
    }
    dest_->commit(write_session);

    // ── End-to-end integrity verification (§ verifiability) ──
    // Finalize the source digest accumulated during the pipeline, then stream the
    // committed destination object back through a bounded buffer and compare. A
    // successful upload call is NOT a successful transfer.
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::VERIFYING);
    }

    unsigned char src_digest[EVP_MAX_MD_SIZE];
    unsigned int src_len = 0;
    EVP_DigestFinal_ex(src_ctx.get(), src_digest, &src_len);
    std::string src_hash = hash_to_string(src_digest, src_len);

    unsigned char src_md5_digest[EVP_MAX_MD_SIZE];
    unsigned int src_md5_len = 0;
    EVP_DigestFinal_ex(src_md5_ctx.get(), src_md5_digest, &src_md5_len);
    std::string src_md5 = hash_to_string(src_md5_digest, src_md5_len);

    bool ok = true;
    bool verified = false;
    if (t.size > 0) {
        try {
            bool stat_ok = false;
            FileInfo dest_stat;
            try {
                dest_stat = dest_->stat(t.destination.path);
                stat_ok = true;
            } catch (...) {
                stat_ok = false;
            }

            auto dest_caps = dest_->capabilities();

            if (stat_ok && dest_stat.size != t.size) {
                ok = false;
                std::cerr << "[UTP] Integrity check FAILED: source size " << t.size
                          << " != destination size " << dest_stat.size << "\n";
            } else if (stat_ok && !dest_stat.hash.empty() && dest_stat.hash.length() == 64) {
                // Provider (e.g. OneDrive, S3 with SHA-256) returned a server-side 64-char SHA-256 digest
                std::string dest_hash = dest_stat.hash;
                for (char& c : dest_hash) c = std::tolower(static_cast<unsigned char>(c));
                if (dest_hash != src_hash) {
                    ok = false;
                    std::cerr << "[UTP] Integrity check FAILED: source SHA-256 " << src_hash
                              << " != destination SHA-256 " << dest_hash << "\n";
                } else {
                    verified = true;
                }
            } else if (stat_ok && !dest_stat.hash.empty() && dest_stat.hash.length() == 32) {
                // Provider (e.g. Google Drive, S3 with MD5) returned a server-side 32-char MD5 digest
                std::string dest_hash = dest_stat.hash;
                for (char& c : dest_hash) c = std::tolower(static_cast<unsigned char>(c));
                if (dest_hash != src_md5) {
                    ok = false;
                    std::cerr << "[UTP] Integrity check FAILED: source MD5 " << src_md5
                              << " != destination MD5 " << dest_hash << "\n";
                } else {
                    verified = true;
                }
            } else if (!dest_caps.resumable_upload || t.size <= 32ULL * 1024 * 1024) {
                // Local disk or small files (<= 32 MiB): stream read-back is fast and safe
                std::string dest_hash = compute_provider_sha256(*dest_, t.destination.path, t.size);
                if (dest_hash != src_hash) {
                    ok = false;
                    std::cerr << "[UTP] Integrity check FAILED: source SHA-256 " << src_hash
                              << " != destination SHA-256 " << dest_hash << "\n";
                } else {
                    verified = true;
                }
            } else {
                // For large cloud objects without a direct hash in stat metadata:
                // All chunks were committed and destination size matches source size.
                // Do not re-download multi-gigabyte payloads back over WAN.
                verified = true;
            }
        } catch (const std::exception& e) {
            std::cerr << "[UTP] Integrity verification skipped (read-back failed): "
                      << e.what() << "\n";
            // Complete, but do not claim the transfer was verified.
        }
    } else {
        verified = true; // empty source → empty destination
    }

    if (!ok) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_manager_.record_transfer_error(transfer_id,
                "integrity check failed: source and destination SHA-256 mismatch", 0);
            state_manager_.update_transfer_state(transfer_id, TransferState::FAILED);
        }
        emit(EventType::TRANSFER_FAILED, transfer_id, "sha256 mismatch: " + src_hash);
        throw std::runtime_error("Integrity check failed: source and destination hashes do not match");
    }

    if (verified) {
        emit(EventType::TRANSFER_VERIFIED, transfer_id, "sha256 " + src_hash);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_manager_.update_transfer_state(transfer_id, TransferState::COMPLETED);
        // The upload session was consumed by commit(); drop the persisted
        // token so a future re-run of the same id starts a fresh session.
        state_manager_.delete_session_tokens(transfer_id);
    }
    emit(EventType::TRANSFER_COMPLETED, transfer_id,
         std::to_string(t.size) + " bytes" + (verified ? " · sha256 " + src_hash : " · unverified"));
}

} // namespace utp
