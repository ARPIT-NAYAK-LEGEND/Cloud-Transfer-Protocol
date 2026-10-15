#pragma once
#include "providers/interface/storage_provider.hpp"
#include <string>
#include <map>
#include <mutex>

#include "providers/common/http_client.hpp"

namespace utp {

class GoogleDriveProvider : public StorageProvider {
public:
    GoogleDriveProvider(const std::string& client_id, const std::string& client_secret = "", const std::string& name_context = "");
    ~GoogleDriveProvider() override;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size) override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;
    std::string get_user_identity() const override { return user_email_; }

    // Server-side copy (zero-bandwidth, direct Google-to-Google copy)
    std::string share_file(const std::string& file_id, const std::string& recipient_email) override;
    bool copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) override;
    void remove_share(const std::string& file_id, const std::string& permission_id) override;

private:
    std::string client_id_;
    std::string client_secret_;
    std::string access_token_;
    std::string refresh_token_;
    std::string user_email_;
    std::string name_context_;
    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;
    
    struct Session {
        std::string id;
        std::string path;
        std::string upload_uri;
        uint64_t total_size{0};
        uint64_t current_offset{0};
    };
    std::map<std::string, Session> sessions_;
    std::map<std::string, std::string> folder_cache_;

    void authenticate();
    std::string ensure_folder_path(const std::string& folder_path);
};

} // namespace utp
