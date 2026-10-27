#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
//  SURFACE STATUS: POST-MVP EXPERIMENTAL EXTENSION (webdav://)
//  This provider adapter is an experimental post-MVP surface pending multi-cloud
//  DoD automation. Core MVP verification gates apply to local://, gdrive://, and onedrive://.
// ═══════════════════════════════════════════════════════════════════════════════
#include "providers/interface/storage_provider.hpp"
#include <string>
#include <map>
#include <mutex>
#include "providers/common/http_client.hpp"

namespace utp {

class WebDAVProvider : public StorageProvider {
public:
    WebDAVProvider(const std::string& base_url = "", 
                   const std::string& username = "", 
                   const std::string& password = "",
                   const std::string& name_context = "");
    ~WebDAVProvider() override;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;
    std::string get_user_identity() const override { return username_; }

    // Server-side copy support (WebDAV COPY method)
    bool copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) override;

private:
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::string auth_header_;
    std::string name_context_;
    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;

    struct Session {
        std::string id;
        std::string path;
        uint64_t total_size{0};
        std::vector<uint8_t> buffer;
    };
    std::map<std::string, Session> sessions_;

    std::string build_full_url(const std::string& path) const;
    void setup_auth();
};

} // namespace utp
