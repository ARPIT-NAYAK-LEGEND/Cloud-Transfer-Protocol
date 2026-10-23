#include "dropbox_provider.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace utp {

namespace {
    static std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        for (char c : value) {
            if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(static_cast<unsigned char>(c));
            }
        }
        return escaped.str();
    }

    static std::string normalize_dropbox_path(const std::string& path) {
        std::string p = path;
        while (!p.empty() && (p.front() == ' ' || p.front() == '\\')) p.erase(0, 1);
        while (!p.empty() && (p.back() == ' ' || p.back() == '/' || p.back() == '\\')) p.pop_back();
        if (p.empty() || p == "root" || p == ".") return "";
        if (p.front() != '/') p = "/" + p;
        return p;
    }

    static std::string escape_json_string(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            if (c == '"') o << "\\\"";
            else if (c == '\\') o << "\\\\";
            else if (c == '\b') o << "\\b";
            else if (c == '\f') o << "\\f";
            else if (c == '\n') o << "\\n";
            else if (c == '\r') o << "\\r";
            else if (c == '\t') o << "\\t";
            else o << c;
        }
        return o.str();
    }

    static std::string extract_json_value(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) pos++;
        if (pos >= json.length()) return "";

        if (json[pos] == '"') {
            pos++; // skip opening quote
            size_t end = pos;
            while (end < json.length()) {
                if (json[end] == '\\' && end + 1 < json.length()) {
                    end += 2;
                } else if (json[end] == '"') {
                    break;
                } else {
                    end++;
                }
            }
            return json.substr(pos, end - pos);
        } else {
            size_t end = pos;
            while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']' &&
                   json[end] != ' ' && json[end] != '\t' && json[end] != '\r' && json[end] != '\n') {
                end++;
            }
            return json.substr(pos, end - pos);
        }
    }

    std::string capture_auth_code_via_server(int port) {
        std::string code;
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) return "";
        
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) return "";
        if (listen(server_fd, 1) < 0) return "";

        struct sockaddr_in client_addr;
#ifdef _WIN32
        int addrlen = sizeof(client_addr);
#else
        socklen_t addrlen = sizeof(client_addr);
#endif
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd >= 0) {
            char buffer[4096] = {0};
            recv(client_fd, buffer, 4096, 0);
            std::string request(buffer);
            
            size_t code_pos = request.find("code=");
            if (code_pos != std::string::npos) {
                code_pos += 5;
                size_t end_pos = request.find('&', code_pos);
                size_t space_pos = request.find(' ', code_pos);
                if (end_pos == std::string::npos || space_pos < end_pos) {
                    end_pos = space_pos;
                }
                code = request.substr(code_pos, end_pos - code_pos);
            }

            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                                   "<html><body style='font-family:sans-serif;text-align:center;padding:50px;background:#0d1117;color:#c9d1d9;'>"
                                   "<h2 style='color:#38bdf8;'>Dropbox Authentication Successful!</h2>"
                                   "<p>You can close this window and return to UTP.</p></body></html>";
            send(client_fd, response.c_str(), response.length(), 0);
            
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
        }
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return code;
    }
    
    void open_browser(const std::string& url) {
#ifdef _WIN32
        std::string cmd = "start \"\" \"" + url + "\"";
#elif __APPLE__
        std::string cmd = "open \"" + url + "\"";
#else
        std::string cmd = "xdg-open \"" + url + "\"";
#endif
        std::system(cmd.c_str());
    }
}

struct DropboxAuthCache {
    std::string access_token;
    std::string refresh_token;
    std::string user_email;
    std::chrono::steady_clock::time_point expires_at;
};
static std::map<std::string, DropboxAuthCache> s_dropbox_cache;
static std::mutex s_dropbox_cache_mutex;

DropboxProvider::DropboxProvider(const std::string& app_key, const std::string& app_secret, const std::string& name_context)
    : app_key_(app_key), app_secret_(app_secret), name_context_(name_context) {
    if (app_key_.empty()) {
        const char* env_k = std::getenv("DROPBOX_APP_KEY");
        if (env_k && strlen(env_k) > 0) app_key_ = env_k;
        else app_key_ = "utp_dropbox_app"; // Standard default placeholder
    }
    if (app_secret_.empty()) {
        const char* env_s = std::getenv("DROPBOX_APP_SECRET");
        if (env_s && strlen(env_s) > 0) app_secret_ = env_s;
    }
    authenticate();
}

DropboxProvider::~DropboxProvider() {}

void DropboxProvider::authenticate() {
    std::string prefix = "\033[1;36m[Dropbox]\033[0m ";
    std::string token_file;
    if (name_context_.empty() || name_context_ == "default" || name_context_ == "src" || name_context_ == "dest") {
        token_file = ".utp_dropbox_token.json";
        std::ifstream test(token_file);
        if (!test.is_open()) {
            std::ifstream test_personal(".utp_dropbox_personal_token.json");
            if (test_personal.is_open()) token_file = ".utp_dropbox_personal_token.json";
        }
    } else {
        token_file = ".utp_dropbox_" + name_context_ + "_token.json";
    }

    // 1. Check in-process static cache first
    {
        std::lock_guard<std::mutex> lock(s_dropbox_cache_mutex);
        auto it = s_dropbox_cache.find(token_file);
        if (it != s_dropbox_cache.end()) {
            if (std::chrono::steady_clock::now() < it->second.expires_at) {
                access_token_ = it->second.access_token;
                refresh_token_ = it->second.refresh_token;
                user_email_ = it->second.user_email;
                return;
            }
        }
    }

    // 2. Read local token file
    std::ifstream infile(token_file);
    if (infile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        access_token_ = extract_json_value(content, "access_token");
        refresh_token_ = extract_json_value(content, "refresh_token");
        user_email_ = extract_json_value(content, "email");
        if (user_email_.empty()) user_email_ = extract_json_value(content, "user_name");

        // Try refreshing if refresh_token is present
        if (!refresh_token_.empty() && !app_key_.empty() && app_key_ != "utp_dropbox_app") {
            std::string url = "https://api.dropboxapi.com/oauth2/token";
            std::string body = "grant_type=refresh_token&refresh_token=" + url_encode(refresh_token_) +
                               "&client_id=" + url_encode(app_key_);
            if (!app_secret_.empty()) {
                body += "&client_secret=" + url_encode(app_secret_);
            }
            std::vector<std::string> h = {"Content-Type: application/x-www-form-urlencoded"};
            auto r = http_.post(url, body, h);

            if (r.status_code == 200) {
                access_token_ = extract_json_value(r.body, "access_token");
                std::string new_ref = extract_json_value(r.body, "refresh_token");
                if (!new_ref.empty()) refresh_token_ = new_ref;

                // Fetch account info
                std::vector<std::string> auth_header = {
                    "Authorization: Bearer " + access_token_,
                    "Content-Type: application/json"
                };
                auto user_resp = http_.post("https://api.dropboxapi.com/2/users/get_current_account", "null", auth_header);
                if (user_resp.status_code == 200) {
                    std::string fetched_email = extract_json_value(user_resp.body, "email");
                    if (!fetched_email.empty()) user_email_ = fetched_email;
                }

                // Save updated token file
                std::string new_json = "{\"access_token\":\"" + access_token_ + "\",\"refresh_token\":\"" + refresh_token_ + "\",\"email\":\"" + user_email_ + "\"}";
                std::ofstream outfile(token_file);
                if (outfile.is_open()) outfile << new_json;

                {
                    std::lock_guard<std::mutex> lock(s_dropbox_cache_mutex);
                    s_dropbox_cache[token_file] = {
                        access_token_,
                        refresh_token_,
                        user_email_,
                        std::chrono::steady_clock::now() + std::chrono::minutes(50)
                    };
                }
                return;
            }
        } else if (!access_token_.empty()) {
            // Verify current access token
            std::vector<std::string> auth_header = {
                "Authorization: Bearer " + access_token_,
                "Content-Type: application/json"
            };
            auto user_resp = http_.post("https://api.dropboxapi.com/2/users/get_current_account", "null", auth_header);
            if (user_resp.status_code == 200) {
                std::string fetched_email = extract_json_value(user_resp.body, "email");
                if (!fetched_email.empty()) user_email_ = fetched_email;
            }
            if (user_email_.empty()) user_email_ = "Connected Dropbox";

            std::lock_guard<std::mutex> lock(s_dropbox_cache_mutex);
            s_dropbox_cache[token_file] = {
                access_token_,
                refresh_token_,
                user_email_,
                std::chrono::steady_clock::now() + std::chrono::minutes(50)
            };
            return;
        }
    }

    // 3. Fallback: interactive browser authorization if credentials are configured
    if (!app_key_.empty() && app_key_ != "utp_dropbox_app") {
        std::cout << prefix << "Authenticating via Dropbox OAuth2...\n";
        std::string redirect_uri = "http://localhost:8080";
        std::string auth_url = "https://www.dropbox.com/oauth2/authorize?"
                               "client_id=" + url_encode(app_key_) +
                               "&response_type=code"
                               "&token_access_type=offline"
                               "&redirect_uri=" + url_encode(redirect_uri);

        std::cout << prefix << "Opening browser for Dropbox authorization...\n";
        open_browser(auth_url);
        std::cout << prefix << "Waiting for authorization code on " << redirect_uri << "...\n";
        std::string auth_code = capture_auth_code_via_server(8080);

        if (!auth_code.empty()) {
            std::string token_url = "https://api.dropboxapi.com/oauth2/token";
            std::string body = "grant_type=authorization_code&code=" + url_encode(auth_code) +
                               "&client_id=" + url_encode(app_key_) +
                               "&redirect_uri=" + url_encode(redirect_uri);
            if (!app_secret_.empty()) {
                body += "&client_secret=" + url_encode(app_secret_);
            }
            std::vector<std::string> headers = {"Content-Type: application/x-www-form-urlencoded"};
            auto resp = http_.post(token_url, body, headers);

            if (resp.status_code == 200) {
                access_token_ = extract_json_value(resp.body, "access_token");
                refresh_token_ = extract_json_value(resp.body, "refresh_token");

                std::vector<std::string> auth_header = {
                    "Authorization: Bearer " + access_token_,
                    "Content-Type: application/json"
                };
                auto user_resp = http_.post("https://api.dropboxapi.com/2/users/get_current_account", "null", auth_header);
                if (user_resp.status_code == 200) {
                    user_email_ = extract_json_value(user_resp.body, "email");
                }
                if (user_email_.empty()) user_email_ = "Connected Dropbox";

                std::string new_json = "{\"access_token\":\"" + access_token_ + "\",\"refresh_token\":\"" + refresh_token_ + "\",\"email\":\"" + user_email_ + "\"}";
                std::ofstream outfile(token_file);
                if (outfile.is_open()) outfile << new_json;

                std::cout << prefix << "Authentication successful! Account: \033[1;32m" << user_email_ << "\033[0m\n";
                {
                    std::lock_guard<std::mutex> lock(s_dropbox_cache_mutex);
                    s_dropbox_cache[token_file] = {
                        access_token_,
                        refresh_token_,
                        user_email_,
                        std::chrono::steady_clock::now() + std::chrono::minutes(50)
                    };
                }
                return;
            }
        }
    }

    // Default mock/demo identity if no token file present yet
    user_email_ = "dropbox-user@utp.cloud";
}

ProviderCapabilities DropboxProvider::capabilities() {
    return {
        true,  // can_stream
        true,  // supports_random_access
        false, // local_path_direct
        true,  // supports_chunking
        true,  // requires_sequential_writes (upload_session is sequential)
        true,  // supports_stat
        true,  // supports_list
        150 * 1024 * 1024, // max_chunk_size (Dropbox supports chunks up to 150MB)
        4      // preferred_chunk_multiple
    };
}

FileInfo DropboxProvider::stat(Path path) {
    std::string norm_path = normalize_dropbox_path(path);
    if (norm_path.empty()) {
        return {"root", 0, true, "Dropbox Root"};
    }

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };

    std::string url = "https://api.dropboxapi.com/2/files/get_metadata";
    std::string body = "{\"path\": \"" + escape_json_string(norm_path) + "\", \"include_media_info\": false}";
    auto resp = http_.post(url, body, headers);

    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Dropbox stat failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::string tag = extract_json_value(resp.body, ".tag");
    std::string name_str = extract_json_value(resp.body, "name");
    std::string size_str = extract_json_value(resp.body, "size");
    bool is_dir = (tag == "folder");
    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

    return {norm_path, size, is_dir, name_str.empty() ? path : name_str};
}

std::vector<FileInfo> DropboxProvider::list(Path path) {
    std::vector<FileInfo> items;
    std::string norm_path = normalize_dropbox_path(path);

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };

    std::string url = "https://api.dropboxapi.com/2/files/list_folder";
    // In Dropbox API, root folder path MUST be ""
    std::string body = "{\"path\": \"" + escape_json_string(norm_path) + "\", \"recursive\": false, \"include_media_info\": false}";

    auto resp = http_.post(url, body, headers);
    if (resp.status_code != 200) {
        std::cerr << "[Dropbox] list failed (" << resp.status_code << "): " << resp.body << "\n";
        return items;
    }

    size_t val_pos = resp.body.find("\"entries\":");
    if (val_pos == std::string::npos) return items;
    size_t arr_start = resp.body.find('[', val_pos);
    if (arr_start == std::string::npos) return items;
    size_t arr_end = resp.body.rfind(']');
    if (arr_end == std::string::npos || arr_end <= arr_start) return items;

    size_t curr = arr_start;
    while (curr < arr_end) {
        size_t obj_start = resp.body.find('{', curr);
        if (obj_start == std::string::npos || obj_start >= arr_end) break;

        int depth = 0;
        size_t obj_end = obj_start;
        bool in_str = false;
        for (size_t i = obj_start; i < arr_end; ++i) {
            if (resp.body[i] == '"' && (i == 0 || resp.body[i - 1] != '\\')) {
                in_str = !in_str;
            } else if (!in_str) {
                if (resp.body[i] == '{') depth++;
                else if (resp.body[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        obj_end = i;
                        break;
                    }
                }
            }
        }
        if (obj_end <= obj_start) break;

        std::string item_json = resp.body.substr(obj_start, obj_end - obj_start + 1);
        std::string tag = extract_json_value(item_json, ".tag");
        std::string name = extract_json_value(item_json, "name");
        std::string path_display = extract_json_value(item_json, "path_display");
        std::string size_str = extract_json_value(item_json, "size");
        bool is_dir = (tag == "folder");
        uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

        if (!name.empty()) {
            std::string item_p = path_display.empty() ? (norm_path + "/" + name) : path_display;
            items.push_back({item_p, size, is_dir, name});
        }

        curr = obj_end + 1;
    }

    return items;
}

ReadStream DropboxProvider::openRead(Path path) {
    std::string norm_path = normalize_dropbox_path(path);
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "dropbox_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, norm_path, "", 0, 0};
    return {id};
}

size_t DropboxProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid Dropbox read session");
        }
        s = it->second;
    }

    uint64_t end_byte = offset + count - 1;
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Dropbox-API-Arg: {\"path\": \"" + escape_json_string(s.path) + "\"}",
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };

    std::string url = "https://content.dropboxapi.com/2/files/download";
    auto resp = http_.post(url, "", headers);

    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0; // EOF
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "Dropbox auth error: " + resp.body, resp.status_code);
        }
        throw TransferError(ErrorCategory::RETRYABLE, "Dropbox read failed: " + resp.body, resp.status_code);
    }

    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count;

    std::memcpy(buffer, resp.body.data(), bytes_read);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session.id].current_offset += bytes_read;
    }

    return bytes_read;
}

WriteSession DropboxProvider::openWrite(Path path, uint64_t size) {
    std::string norm_path = normalize_dropbox_path(path);
    if (norm_path.empty()) norm_path = "/transferred_file";

    if (size == 0) {
        // Create empty file immediately
        std::vector<std::string> headers = {
            "Authorization: Bearer " + access_token_,
            "Content-Type: application/octet-stream",
            "Dropbox-API-Arg: {\"path\": \"" + escape_json_string(norm_path) + "\", \"mode\": \"overwrite\", \"autorename\": false}"
        };
        auto resp = http_.post("https://content.dropboxapi.com/2/files/upload", "", headers);
        if (resp.status_code != 200) {
            throw TransferError(ErrorCategory::FATAL, "Dropbox create empty file failed: " + resp.body);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::string id = "dropbox_write_" + std::to_string(next_id_++);
        sessions_[id] = {id, norm_path, "", 0, 0};
        return {id};
    }

    // Initialize chunked upload session
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/octet-stream",
        "Dropbox-API-Arg: {\"close\": false}"
    };

    auto resp = http_.post("https://content.dropboxapi.com/2/files/upload_session/start", "", headers);
    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Dropbox upload session start failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::string session_id = extract_json_value(resp.body, "session_id");
    if (session_id.empty()) {
        throw TransferError(ErrorCategory::FATAL, "Dropbox missing session_id in upload_session/start response");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "dropbox_write_" + std::to_string(next_id_++);
    sessions_[id] = {id, norm_path, session_id, size, 0};
    return {id};
}

void DropboxProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    if (size == 0) return;
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid Dropbox write session");
        }
        s = it->second;
    }

    if (s.session_id.empty()) return;

    std::string arg = "{\"cursor\": {\"session_id\": \"" + s.session_id + "\", \"offset\": " + std::to_string(offset) + "}, \"close\": false}";
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/octet-stream",
        "Dropbox-API-Arg: " + arg
    };

    auto resp = http_.put("https://content.dropboxapi.com/2/files/upload_session/append_v2", data, size, headers);
    if (resp.status_code != 200) {
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "Dropbox write auth error: " + resp.body, resp.status_code);
        }
        throw TransferError(ErrorCategory::RETRYABLE, "Dropbox upload_session/append failed (" + std::to_string(resp.status_code) + "): " + resp.body, resp.status_code);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session.id].current_offset = offset + size;
    }
}

void DropboxProvider::commit(WriteSession session) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) return;
        s = it->second;
        sessions_.erase(it);
    }

    if (s.session_id.empty()) return; // Already committed (empty file)

    std::string arg = "{\"cursor\": {\"session_id\": \"" + s.session_id + "\", \"offset\": " + std::to_string(s.current_offset) + "},"
                      "\"commit\": {\"path\": \"" + escape_json_string(s.path) + "\", \"mode\": \"overwrite\", \"autorename\": false, \"mute\": false}}";

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/octet-stream",
        "Dropbox-API-Arg: " + arg
    };

    auto resp = http_.post("https://content.dropboxapi.com/2/files/upload_session/finish", "", headers);
    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Dropbox upload_session/finish failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }
}

void DropboxProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SERVER-SIDE COPY (zero-bandwidth direct Dropbox-to-Dropbox clone)
// ═══════════════════════════════════════════════════════════════════════════════

std::string DropboxProvider::share_file(const std::string& /*file_id*/, const std::string& /*recipient_email*/) {
    // Dropbox same-account copy does not require external user sharing
    return "";
}

bool DropboxProvider::copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) {
    std::string from_p = normalize_dropbox_path(source_file_id);
    std::string to_p = normalize_dropbox_path(dest_path);

    if (to_p.empty() || to_p == "/") {
        std::string fname = filename.empty() ? "copied_file" : filename;
        while (!fname.empty() && fname.front() == '/') fname.erase(0, 1);
        to_p = "/" + fname;
    } else if (to_p.find('.') == std::string::npos && !filename.empty()) {
        std::string fname = filename;
        while (!fname.empty() && fname.front() == '/') fname.erase(0, 1);
        to_p = to_p + "/" + fname;
    }

    std::string url = "https://api.dropboxapi.com/2/files/copy_v2";
    std::string body = "{\"from_path\": \"" + escape_json_string(from_p) + "\", "
                       "\"to_path\": \"" + escape_json_string(to_p) + "\", "
                       "\"autorename\": true}";

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };

    auto resp = http_.post(url, body, headers);
    return (resp.status_code == 200);
}

void DropboxProvider::remove_share(const std::string& /*file_id*/, const std::string& /*permission_id*/) {
    // No share to clean up for direct copy_v2
}

} // namespace utp
