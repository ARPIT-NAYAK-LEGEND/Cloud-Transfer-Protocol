#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
//  HTTP / HTTPS DIRECT STREAM INGESTION PROVIDER (http://, https://)
//  Streams public and authenticated web downloads directly into cloud targets
//  via in-memory chunked Range pipelines with zero local disk footprint.
// ═══════════════════════════════════════════════════════════════════════════════

#include "providers/interface/storage_provider.hpp"
#include "providers/common/http_client.hpp"
#include <string>
#include <map>
#include <mutex>
#include <vector>

namespace utp {

class HttpProvider : public StorageProvider {
public:
    HttpProvider(const std::string& base_url = "", const std::string& auth_context = "", bool allow_private_hosts = false);
    ~HttpProvider() override = default;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;
    std::string get_user_identity() const override { return "web-direct"; }

private:
    std::string base_url_;
    std::string auth_context_; // Token or credentials
    bool allow_private_hosts_{false};
    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;

    struct Session {
        std::string id;
        std::string url;
        uint64_t total_size{0};
    };
    std::map<std::string, Session> sessions_;

    std::string resolve_url(const std::string& path) const;
    static std::string extract_filename(const std::map<std::string, std::string>& headers, const std::string& url);
};

} // namespace utp
