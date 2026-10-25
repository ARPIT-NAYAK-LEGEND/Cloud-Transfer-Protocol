#pragma once

#include "providers/interface/storage_provider.hpp"
#include <fstream>
#include <map>
#include <mutex>

namespace utp {

class LocalProvider : public StorageProvider {
public:
    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;
    ~LocalProvider() override;

private:
    std::mutex mutex_;
    uint64_t next_id_ = 1;
    
    struct ReadState {
        std::ifstream stream;
    };
    
    struct WriteState {
        std::ofstream stream;
        Path path;
        Path temp_path;
    };
    
    std::map<std::string, ReadState> read_streams_;
    std::map<std::string, WriteState> write_sessions_;
};

} // namespace utp
