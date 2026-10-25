#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utp {

enum class TransferState {
    CREATED,
    AUTHENTICATING,
    DISCOVERING,
    NEGOTIATING,
    PLANNING,
    TRANSFERRING,
    VERIFYING,
    COMMITTING,
    COMPLETED,
    PAUSED,
    RETRYING,
    REPAIRING,
    CANCELLED,
    FAILED
};

enum class ChunkState {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    FAILED
};

struct ResourceInfo {
    std::string provider;
    std::string path;
};

struct Transfer {
    std::string id;
    ResourceInfo source;
    ResourceInfo destination;
    uint64_t size;
    TransferState state;
    uint64_t bytes_transferred;
    uint64_t chunk_size;
};

struct ProviderCapabilities {
    bool ranged_read;
    bool resumable_upload;
    bool multipart_upload;
    bool server_side_copy;
    bool checksum;
    bool metadata;
    bool directory_support;
    uint64_t max_chunk_size;
    uint32_t max_parallel_requests;
};

struct FileInfo {
    std::string path;
    uint64_t size;
    bool is_directory;
    std::string name = "";
};

struct ChunkInfo {
    uint64_t offset;
    uint64_t size;
    ChunkState state;
    uint32_t attempts{0};
};

// §8 Error classification
enum class ErrorCategory {
    RETRYABLE,   // retry with exponential backoff + jitter
    AUTH,        // refresh / re-authenticate
    RATE_LIMIT,  // provider-aware backoff
    FATAL        // fail, surface to user
};

struct TransferError : public std::exception {
    ErrorCategory category;
    std::string message;
    int http_status{0};

    TransferError(ErrorCategory cat, const std::string& msg, int status = 0)
        : category(cat), message(msg), http_status(status) {}

    const char* what() const noexcept override { return message.c_str(); }
};

struct RetryConfig {
    uint32_t max_retries{3};
    uint32_t base_delay_ms{500};
    uint32_t max_delay_ms{30000};
    double jitter_factor{0.25};
};

} // namespace utp
