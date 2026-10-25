#pragma once
#include "providers/interface/storage_provider.hpp"
#include "providers/common/http_client.hpp"
#include <string>
#include <map>
#include <mutex>
#include <vector>

namespace utp {

class AzureBlobProvider : public StorageProvider {
public:
    AzureBlobProvider();
    ~AzureBlobProvider() override;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;

private:
    std::string account_name_;
    std::string account_key_;
    std::string endpoint_;

    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;
    
    struct Session {
        std::string id;
        std::string path;
        std::map<uint32_t, std::string> block_ids;
    };
    std::map<std::string, Session> sessions_;

    std::string sign_shared_key(const std::string& method, const std::string& url, const std::vector<std::string>& headers);
};

} // namespace utp
