#pragma once

#include "providers/interface/storage_provider.hpp"
#include <map>
#include <atomic>
#include <stdexcept>

namespace utp {

class MockProvider : public StorageProvider {
public:
    ProviderCapabilities capabilities() override {
        return {
            true,  // ranged_read
            true,  // resumable_upload
            true,  // multipart_upload
            true,  // server_side_copy
            true,  // checksum
            true,  // metadata
            true,  // directory_support
            8388608, // max_chunk_size
            4      // max_parallel_requests
        };
    }

    FileInfo stat(Path path) override {
        auto it = files.find(path);
        if (it == files.end()) {
            throw std::runtime_error("File not found");
        }
        return {path, it->second, false};
    }

    std::vector<FileInfo> list(Path path) override {
        (void)path;
        return {};
    }

    ReadStream openRead(Path path) override {
        return {"read_" + path};
    }

    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override {
        (void)size;
        (void)resume_token;
        last_open_write_path = path;
        return {"write_" + path};
    }

    std::string write_session_token(WriteSession session) override {
        // Model a provider that hands the engine a durable token: proves the
        // engine persists and round-trips it through openWrite on resume.
        return "mock_session:" + session.id;
    }

    void refresh_auth() override {
        refresh_count_++;
    }

    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override {
        (void)session;
        (void)offset;
        if (auth_fail_pending_read_.exchange(false)) {
            throw TransferError(ErrorCategory::AUTH, "401 simulated: expired token on read");
        }
        if (inject_failures_ && (rand() % 100 < fail_rate_)) {
            if (rand() % 2 == 0) throw TransferError(ErrorCategory::RATE_LIMIT, "429 Too Many Requests");
            else throw TransferError(ErrorCategory::RETRYABLE, "Connection Reset");
        }
        return count;
    }

    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override {
        (void)offset;
        (void)data;
        if (auth_fail_pending_write_.exchange(false)) {
            throw TransferError(ErrorCategory::AUTH, "401 simulated: expired token on write");
        }
        if (auth_persist_after_refresh_ && refresh_count_.load() == 0) {
            throw TransferError(ErrorCategory::AUTH, "401 persistent: refresh not attempted yet");
        }
        if (inject_failures_ && (rand() % 100 < fail_rate_)) {
            if (rand() % 2 == 0) throw TransferError(ErrorCategory::RATE_LIMIT, "429 Too Many Requests");
            else throw TransferError(ErrorCategory::RETRYABLE, "Connection Timeout");
        }
        (void)session;
    }

    void commit(WriteSession session) override {
        (void)session;
    }

    void abort(WriteSession session) override {
        (void)session;
    }

    // ── test helpers ──

    void add_file(Path path, uint64_t size) {
        files[path] = size;
    }

    void enable_chaos(int fail_rate) {
        inject_failures_ = true;
        fail_rate_ = fail_rate;
    }

    // First write after this call throws AUTH exactly once — the engine must
    // call refresh_auth() and retry with the "fresh" token.
    void inject_auth_failure_once() { auth_fail_pending_write_ = true; }

    // Every write throws AUTH until refresh_auth() has been called at least
    // once — proves a provider that keeps 401ing is failed, not retried forever.
    void require_refresh_before_accepting() { auth_persist_after_refresh_ = true; }

    void inject_auth_failure_once_on_read() { auth_fail_pending_read_ = true; }

    int refresh_count() const { return refresh_count_.load(); }

    std::string last_open_write_path;

private:
    std::map<Path, uint64_t> files;
    bool inject_failures_ = false;
    int fail_rate_ = 0;
    std::atomic<int> refresh_count_{0};
    std::atomic<bool> auth_fail_pending_write_{false};
    std::atomic<bool> auth_fail_pending_read_{false};
    std::atomic<bool> auth_persist_after_refresh_{false};
};

} // namespace utp
