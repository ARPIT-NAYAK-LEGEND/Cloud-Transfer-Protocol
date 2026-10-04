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
    virtual size_t read(ReadStream session, uint8_t* buffer, size_t count) = 0;
    virtual void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) = 0;
    virtual void commit(WriteSession session) = 0;
    virtual void abort(WriteSession session) = 0;
    virtual ~StorageProvider() = default;
};

} // namespace utp
