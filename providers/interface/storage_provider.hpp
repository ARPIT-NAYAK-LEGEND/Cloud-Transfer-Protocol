#pragma once

#include "utp/types.hpp"
#include <string>
#include <vector>

namespace utp {

// Opaque types for stream/session - will be expanded in later milestones
struct ReadStream {
    std::string id;
};

struct WriteSession {
    std::string id;
};

using Path = std::string;

class StorageProvider {
public:
    virtual ProviderCapabilities capabilities() = 0;
    virtual FileInfo stat(Path path) = 0;
    virtual std::vector<FileInfo> list(Path path) = 0;
    virtual ReadStream openRead(Path path) = 0;
    virtual WriteSession openWrite(Path path, uint64_t size) = 0;
    virtual size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) = 0;
    virtual void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) = 0;
    virtual void commit(WriteSession session) = 0;
    virtual void abort(WriteSession session) = 0;
    virtual std::string get_user_identity() const { return ""; }

    // ── Server-Side Copy (zero-bandwidth, direct cloud copy) ──
    // Share a file with another user's email; returns permission ID (empty = unsupported/failed)
    virtual std::string share_file(const std::string& /*file_id*/, const std::string& /*recipient_email*/) { return ""; }
    // Copy a file that's been shared with this user into dest_path; returns true on success
    virtual bool copy_shared_file(const std::string& /*source_file_id*/, const std::string& /*dest_path*/, const std::string& /*filename*/) { return false; }
    // Remove a share permission from a file
    virtual void remove_share(const std::string& /*file_id*/, const std::string& /*permission_id*/) {}

    virtual ~StorageProvider() = default;
};

} // namespace utp
