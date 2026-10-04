#pragma once

#include "providers/interface/storage_provider.hpp"
#include <map>
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
        if (files.find(path) == files.end()) {
            throw std::runtime_error("File not found");
        }
        return {path, files[path], false};
    }

    std::vector<FileInfo> list(Path path) override {
        return {};
    }

    ReadStream openRead(Path path) override {
        return {"read_" + path};
    }

    WriteSession openWrite(Path path, uint64_t size) override {
        return {"write_" + path};
    }

    void commit(WriteSession session) override {
        // dummy commit
    }

    size_t read(ReadStream session, uint8_t* buffer, size_t count) override {
        return 0;
    }

    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override {
    }

    void abort(WriteSession session) override {
        // dummy abort
    }

    // test helper
    void add_file(Path path, uint64_t size) {
        files[path] = size;
    }

private:
    std::map<Path, uint64_t> files;
};

} // namespace utp
