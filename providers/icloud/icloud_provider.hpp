#pragma once
#include "providers/interface/storage_provider.hpp"
#include "providers/common/http_client.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

namespace utp {

class ICloudProvider : public StorageProvider {
public:
    ICloudProvider(const std::string& apple_id = "", 
                   const std::string& auth_token = "",
                   const std::string& name_context = "");
    ~ICloudProvider() override;

    ProviderCapabilities capabilities() override;
    FileInfo stat(Path path) override;
    std::vector<FileInfo> list(Path path) override;
    ReadStream openRead(Path path) override;
    WriteSession openWrite(Path path, uint64_t size, const std::string& resume_token = "") override;
    size_t read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) override;
    void write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) override;
    void commit(WriteSession session) override;
    void abort(WriteSession session) override;
    std::string get_user_identity() const override;

    // Direct cloud-to-cloud server side copy (Web API or local)
    bool copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) override;

    // Check if local iCloud Drive directory is available on this system
    static bool is_local_bridge_available();
    static std::string get_local_bridge_path();

    // Authenticate with Apple ID / token
    bool authenticate_web(const std::string& apple_id, const std::string& session_token);

private:
    std::string apple_id_;
    std::string auth_token_;       // dsWebAuthToken or session token
    std::string name_context_;
    std::string dsid_;             // Apple Directory Services ID
    std::string docws_url_;        // e.g. "https://p58-docws.icloud.com:443"
    std::string cookies_;          // full cookie header
    bool use_local_bridge_{false};
    std::string local_root_path_;

    std::mutex mutex_;
    uint64_t next_id_{1};
    HttpClient http_;

    struct Session {
        std::string id;
        std::string path;
        std::string download_url;
        uint64_t total_size{0};
        std::vector<uint8_t> buffer;
    };
    std::map<std::string, Session> sessions_;

    void init_session();
    void load_token_file();
    void save_token_file(const std::string& apple_id, const std::string& token, const std::string& dsid, const std::string& docws_url);
    std::string get_token_file_path() const;

    // Path to drivewsid resolution
    std::string resolve_drivewsid(const std::string& path);
};

} // namespace utp
