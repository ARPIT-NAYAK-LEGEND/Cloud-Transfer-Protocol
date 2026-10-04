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
};

struct ChunkInfo {
    uint64_t offset;
    uint64_t size;
    ChunkState state;
};

} // namespace utp
