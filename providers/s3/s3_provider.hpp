#pragma once
#include "providers/interface/storage_provider.hpp"
#include "providers/common/http_client.hpp"
#include <string>
#include <map>
#include <mutex>
#include <vector>

namespace utp {

class S3Provider : public StorageProvider {
public:
    S3Provider(const std::string& access_key = "", 
               const std::string& secret_key = "", 
               const std::string& region = "",
               const std::string& custom_endpoint = "");
    ~S3Provider() override;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size) override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;

private:
    std::string access_key_;
    std::string secret_key_;
    std::string region_;
    std::string endpoint_;

    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;
    
    struct Session {
        std::string id;
        std::string path;
        std::string upload_id;
        std::map<uint32_t, std::string> etags; // part_number -> ETag
    };
    std::map<std::string, Session> sessions_;

    struct SignedRequest {
        std::string url;
        std::vector<std::string> headers;
    };

    SignedRequest build_signed_request(const std::string& method, 
                                       const std::string& url, 
                                       const uint8_t* payload, 
                                       size_t payload_len, 
                                       const std::vector<std::string>& extra_headers);
};

} // namespace utp
