#include "gdrive_provider.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
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
            
            // parse GET /?code=... HTTP/1.1
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

            std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h2>Authentication successful! You can close this window and return to the application.</h2></body></html>";
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


static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << int(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
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

GoogleDriveProvider::GoogleDriveProvider(const std::string& client_id, const std::string& client_secret, const std::string& name_context) 
    : client_id_(client_id), client_secret_(client_secret), name_context_(name_context) {
    authenticate();
}

GoogleDriveProvider::~GoogleDriveProvider() {}

struct GDriveAuthCache {
    std::string access_token;
    std::string refresh_token;
    std::string email;
    std::chrono::steady_clock::time_point expires_at;
};
static std::map<std::string, GDriveAuthCache> s_gdrive_cache;
static std::mutex s_gdrive_cache_mutex;

void GoogleDriveProvider::authenticate() {
    std::string prefix = "\033[1;36m[GoogleDrive]\033[0m ";
    std::string token_file;
    if (name_context_.empty() || name_context_ == "default" || name_context_ == "src" || name_context_ == "dest") {
        token_file = ".utp_gdrive_my_personal_token.json";
        std::ifstream test(token_file);
        if (!test.is_open()) {
            token_file = ".utp_gdrive_default_token.json";
        }
    } else {
        token_file = ".utp_gdrive_" + name_context_ + "_token.json";
    }
    
    std::ifstream infile(token_file);

    // 1. Check in-process memory cache first (instantaneous, zero network, zero stdout)
    {
        std::lock_guard<std::mutex> lock(s_gdrive_cache_mutex);
        auto it = s_gdrive_cache.find(token_file);
        if (it != s_gdrive_cache.end()) {
            if (std::chrono::steady_clock::now() < it->second.expires_at) {
                access_token_ = it->second.access_token;
                refresh_token_ = it->second.refresh_token;
                user_email_ = it->second.email;
                return;
            }
        }
    }

    if (infile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        access_token_ = extract_json_value(content, "access_token");
        refresh_token_ = extract_json_value(content, "refresh_token");
        user_email_ = extract_json_value(content, "email");
        
        if (!refresh_token_.empty()) {
            std::string url = "https://oauth2.googleapis.com/token";
            std::string body = "client_id=" + client_id_ + "&client_secret=" + client_secret_ + "&refresh_token=" + refresh_token_ + "&grant_type=refresh_token";
            std::vector<std::string> h = {"Content-Type: application/x-www-form-urlencoded"};
            auto r = http_.post(url, body, h);
            
            if (r.status_code == 200) {
                access_token_ = extract_json_value(r.body, "access_token");
                std::string email = user_email_.empty() ? "Connected Account" : user_email_;
                std::vector<std::string> auth_header = {"Authorization: Bearer " + access_token_};
                auto user_resp = http_.get("https://www.googleapis.com/drive/v3/about?fields=user(emailAddress)", auth_header);
                if (user_resp.status_code == 200) {
                    std::string fetched_email = extract_json_value(user_resp.body, "emailAddress");
                    if (!fetched_email.empty()) email = fetched_email;
                }
                user_email_ = email;
                
                // Save updated token file with email
                std::string new_json = "{\"access_token\":\"" + access_token_ + "\",\"refresh_token\":\"" + refresh_token_ + "\",\"email\":\"" + email + "\"}";
                std::ofstream outfile(token_file);
                if (outfile.is_open()) outfile << new_json;

                // Cache in memory for 50 minutes
                {
                    std::lock_guard<std::mutex> lock(s_gdrive_cache_mutex);
                    s_gdrive_cache[token_file] = {
                        access_token_,
                        refresh_token_,
                        email,
                        std::chrono::steady_clock::now() + std::chrono::minutes(50)
                    };
                }
                return;
            }
        } else if (!access_token_.empty()) {
            std::lock_guard<std::mutex> lock(s_gdrive_cache_mutex);
            s_gdrive_cache[token_file] = {
                access_token_,
                refresh_token_,
                user_email_.empty() ? "Connected Account" : user_email_,
                std::chrono::steady_clock::now() + std::chrono::minutes(50)
            };
            return;
        }
    }

    std::cout << prefix << "Authenticating via OAuth2...\n";
    
    std::string redirect_uri = "http://127.0.0.1:8080";
    std::string auth_url = "https://accounts.google.com/o/oauth2/v2/auth?"
                           "client_id=" + client_id_ +
                           "&redirect_uri=" + redirect_uri +
                           "&response_type=code"
                           "&scope=https://www.googleapis.com/auth/drive"
                           "&access_type=offline"
                           "&prompt=consent%20select_account";

    std::cout << prefix << "Opening browser for authentication...\n";
    open_browser(auth_url);
    
    std::cout << prefix << "Waiting for authorization code on http://127.0.0.1:8080...\n";
    std::string auth_code = capture_auth_code_via_server(8080);
    
    if (auth_code.empty()) {
        throw std::runtime_error("Failed to capture authorization code from local server");
    }

    // Exchange the authorization code for access and refresh tokens
    std::string token_url = "https://oauth2.googleapis.com/token";
    std::string body = "code=" + auth_code + 
                       "&client_id=" + client_id_ + 
                       "&client_secret=" + client_secret_ + 
                       "&redirect_uri=" + redirect_uri + 
                       "&grant_type=authorization_code";
                       
    std::vector<std::string> headers = {
        "Content-Type: application/x-www-form-urlencoded"
    };

    auto resp = http_.post(token_url, body, headers);
    
    if (resp.status_code == 200) {
        access_token_ = extract_json_value(resp.body, "access_token");
        refresh_token_ = extract_json_value(resp.body, "refresh_token");
        
        std::string email = "Connected Account";
        std::vector<std::string> auth_header = {"Authorization: Bearer " + access_token_};
        auto user_resp = http_.get("https://www.googleapis.com/drive/v3/about?fields=user(emailAddress)", auth_header);
        if (user_resp.status_code == 200) {
            std::string fetched_email = extract_json_value(user_resp.body, "emailAddress");
            if (!fetched_email.empty()) email = fetched_email;
        }
        user_email_ = email;

        std::string new_json = "{\"access_token\":\"" + access_token_ + "\",\"refresh_token\":\"" + refresh_token_ + "\",\"email\":\"" + email + "\"}";
        std::ofstream outfile(token_file);
        if (outfile.is_open()) {
            outfile << new_json;
        }

        {
            std::lock_guard<std::mutex> lock(s_gdrive_cache_mutex);
            s_gdrive_cache[token_file] = {
                access_token_,
                refresh_token_,
                email,
                std::chrono::steady_clock::now() + std::chrono::minutes(50)
            };
        }
        
        std::cout << prefix << "Authentication successful! Connected as " << email << "\n";
    } else {
        std::cerr << "[GoogleDrive] Authentication failed (" << resp.status_code << "): " << resp.body << "\n";
        throw std::runtime_error("OAuth authentication failed");
    }
}

ProviderCapabilities GoogleDriveProvider::capabilities() {
    return {
        true,  true,  true,  true,  true,  true,  true,
        1024 * 1024 * 1024, 4
    };
}

static std::string sanitize_gdrive_id(std::string input) {
    while (!input.empty() && (input.front() == ' ' || input.front() == '\t' || input.front() == '"' || input.front() == '\'')) input.erase(0, 1);
    while (!input.empty() && (input.back() == ' ' || input.back() == '\t' || input.back() == '\r' || input.back() == '\n' || input.back() == '"' || input.back() == '\'')) input.pop_back();

    size_t f_pos = input.find("/folders/");
    if (f_pos != std::string::npos) {
        size_t start = f_pos + 9;
        size_t end = input.find('/', start);
        if (end == std::string::npos) end = input.find('?', start);
        if (end == std::string::npos) end = input.size();
        return input.substr(start, end - start);
    }
    size_t d_pos = input.find("/file/d/");
    if (d_pos != std::string::npos) {
        size_t start = d_pos + 8;
        size_t end = input.find('/', start);
        if (end == std::string::npos) end = input.find('?', start);
        if (end == std::string::npos) end = input.size();
        return input.substr(start, end - start);
    }
    size_t id_pos = input.find("id=");
    if (id_pos != std::string::npos) {
        size_t start = id_pos + 3;
        size_t end = input.find('&', start);
        if (end == std::string::npos) end = input.size();
        return input.substr(start, end - start);
    }
    size_t q_pos = input.find('?');
    if (q_pos != std::string::npos) input = input.substr(0, q_pos);
    return input;
}

FileInfo GoogleDriveProvider::stat(Path path) {
    std::string clean_id = sanitize_gdrive_id(path);
    if (clean_id.empty() || clean_id == "root" || clean_id == "/") {
        return {"root", 0, true, "root"};
    }
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_
    };
    std::string url = "https://www.googleapis.com/drive/v3/files/" + clean_id + "?fields=id,name,size,mimeType";
    auto resp = http_.get(url, headers);
    
    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Google Drive stat failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::string size_str = extract_json_value(resp.body, "size");
    std::string name_str = extract_json_value(resp.body, "name");
    std::string mime_str = extract_json_value(resp.body, "mimeType");
    bool is_dir = (mime_str == "application/vnd.google-apps.folder");
    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);
    return {clean_id, size, is_dir, name_str.empty() ? clean_id : name_str};
}

std::vector<FileInfo> GoogleDriveProvider::list(Path path) {
    std::vector<FileInfo> items;
    std::string parent_id = sanitize_gdrive_id(path);
    if (parent_id.empty() || parent_id == "/" || parent_id == ".") {
        parent_id = "root";
    }

    std::string q = "'" + parent_id + "' in parents and trashed = false";
    std::string url = "https://www.googleapis.com/drive/v3/files?pageSize=100&fields=files(id,name,mimeType,size)&q=" + url_encode(q);
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_
    };

    auto resp = http_.get(url, headers);
    if (resp.status_code != 200) {
        std::cerr << "[GoogleDrive] list failed (" << resp.status_code << "): " << resp.body << "\n";
        return items;
    }

    size_t files_pos = resp.body.find("\"files\":");
    if (files_pos == std::string::npos) return items;
    size_t arr_start = resp.body.find('[', files_pos);
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
        std::string id = extract_json_value(item_json, "id");
        std::string name = extract_json_value(item_json, "name");
        std::string mime = extract_json_value(item_json, "mimeType");
        std::string size_str = extract_json_value(item_json, "size");

        bool is_dir = (mime == "application/vnd.google-apps.folder");
        uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

        if (!id.empty() && !name.empty()) {
            items.push_back({id, size, is_dir, name});
        }

        curr = obj_end + 1;
    }

    return items;
}

ReadStream GoogleDriveProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string clean_id = sanitize_gdrive_id(path);
    std::string id = "gdrive_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, clean_id, "", 0, 0};
    return {id};
}

std::string GoogleDriveProvider::ensure_folder_path(const std::string& folder_path) {
    if (folder_path.empty() || folder_path == "root" || folder_path == ".") return "root";
    if (folder_path.find('/') == std::string::npos && folder_path.length() > 20) {
        return folder_path; // Already an ID
    }

    std::stringstream ss(folder_path);
    std::string segment;
    std::string current_parent = "root";

    while (std::getline(ss, segment, '/')) {
        while (!segment.empty() && (segment.front() == ' ' || segment.front() == '/')) segment.erase(0, 1);
        while (!segment.empty() && (segment.back() == ' ' || segment.back() == '/')) segment.pop_back();
        if (segment.empty() || segment == "." || segment == "root") continue;

        std::string cache_key = current_parent + "::" + segment;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = folder_cache_.find(cache_key);
            if (it != folder_cache_.end()) {
                current_parent = it->second;
                continue;
            }
        }

        // Search for folder in current_parent
        std::string query = "name = '" + segment + "' and '" + current_parent + "' in parents and mimeType = 'application/vnd.google-apps.folder' and trashed = false";
        std::string url = "https://www.googleapis.com/drive/v3/files?q=" + url_encode(query) + "&fields=files(id,name)&spaces=drive";
        std::vector<std::string> headers = {"Authorization: Bearer " + access_token_};
        auto resp = http_.get(url, headers);

        std::string found_id = "";
        if (resp.status_code == 200) {
            found_id = extract_json_value(resp.body, "id");
        }

        if (found_id.empty()) {
            // Create folder
            std::string create_url = "https://www.googleapis.com/drive/v3/files";
            std::string body = "{\"name\":\"" + segment + "\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"" + current_parent + "\"]}";
            std::vector<std::string> post_headers = {
                "Authorization: Bearer " + access_token_,
                "Content-Type: application/json"
            };
            auto create_resp = http_.post(create_url, body, post_headers);
            if (create_resp.status_code == 200 || create_resp.status_code == 201) {
                found_id = extract_json_value(create_resp.body, "id");
            }
        }

        if (!found_id.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            folder_cache_[cache_key] = found_id;
            current_parent = found_id;
        }
    }

    return current_parent;
}

WriteSession GoogleDriveProvider::openWrite(Path path, uint64_t size) {
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "X-Upload-Content-Length: " + std::to_string(size),
        "Content-Type: application/json"
    };
    
    std::string filename = path;
    std::string folder_id = "";
    size_t slash_pos = path.find_last_of('/');
    if (slash_pos != std::string::npos) {
        std::string folder_path = path.substr(0, slash_pos);
        filename = path.substr(slash_pos + 1);
        try {
            folder_id = ensure_folder_path(folder_path);
        } catch (...) {}
    }
    
    std::string url = "https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable";
    std::string body = "{\"name\":\"" + filename + "\"";
    if (!folder_id.empty()) {
        body += ",\"parents\":[\"" + folder_id + "\"]";
    }
    body += "}";
    
    auto resp = http_.post(url, body, headers);

    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Google Drive resumable init failed: " + std::to_string(resp.status_code) + " - " + resp.body);
    }

    auto loc_it = resp.headers.find("Location");
    if (loc_it == resp.headers.end()) {
        loc_it = resp.headers.find("location");
    }
    
    if (loc_it == resp.headers.end()) {
        throw TransferError(ErrorCategory::FATAL, "Google Drive resumable init missing Location header");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "gdrive_write_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, loc_it->second, size};
    return {id};
}

size_t GoogleDriveProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid Google Drive read session");
        }
        s = sessions_[session.id];
    }
    
    uint64_t end_byte = offset + count - 1;
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };
    
    std::string url = "https://www.googleapis.com/drive/v3/files/" + s.path + "?alt=media";
    auto resp = http_.get(url, headers);
    
    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0; // Range Not Satisfiable (EOF)
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "Google Drive auth error: " + resp.body, resp.status_code);
        }
        throw TransferError(ErrorCategory::RETRYABLE, "Google Drive read failed: " + resp.body, resp.status_code);
    }
    
    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count; // Safeguard
    
    std::memcpy(buffer, resp.body.data(), bytes_read);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session.id].current_offset += bytes_read;
    }
    
    return bytes_read;
}

void GoogleDriveProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid Google Drive write session");
        }
        s = sessions_[session.id];
    }

    uint64_t end_byte = offset + size - 1;
    std::vector<std::string> headers = {
        "Content-Length: " + std::to_string(size),
        "Content-Range: bytes " + std::to_string(offset) + "-" + std::to_string(end_byte) + "/" + std::to_string(s.total_size)
    };

    auto resp = http_.put(s.upload_uri, data, size, headers);
    
    // 308 means incomplete (more chunks), 200/201 means complete
    if (resp.status_code != 308 && resp.status_code != 200 && resp.status_code != 201) {
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "Google Drive auth error: " + resp.body, resp.status_code);
        }
        if (resp.status_code == 429) {
            throw TransferError(ErrorCategory::RATE_LIMIT, "Google Drive rate limit: " + resp.body, resp.status_code);
        }
        throw TransferError(ErrorCategory::RETRYABLE, "Google Drive write failed: " + resp.body, resp.status_code);
    }
}

void GoogleDriveProvider::commit(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

void GoogleDriveProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SERVER-SIDE COPY (zero-bandwidth, direct Google-to-Google)
// ═══════════════════════════════════════════════════════════════════════════════

std::string GoogleDriveProvider::share_file(const std::string& file_id, const std::string& recipient_email) {
    std::string clean_id = sanitize_gdrive_id(file_id);
    // Create a temporary "reader" permission — sendNotificationEmail=false to avoid spam
    std::string url = "https://www.googleapis.com/drive/v3/files/" + clean_id +
                      "/permissions?sendNotificationEmail=false&supportsAllDrives=true";
    std::string body = "{\"role\":\"reader\",\"type\":\"user\",\"emailAddress\":\"" + recipient_email + "\"}";
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };
    auto resp = http_.post(url, body, headers);
    if (resp.status_code == 200 || resp.status_code == 201) {
        return extract_json_value(resp.body, "id");
    }
    return "";
}

bool GoogleDriveProvider::copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) {
    std::string clean_source_id = sanitize_gdrive_id(source_file_id);

    std::string dest_filename = filename;
    std::string folder_id = "";
    size_t slash_pos = dest_path.find_last_of('/');
    if (slash_pos != std::string::npos) {
        std::string folder_path = dest_path.substr(0, slash_pos);
        if (dest_filename.empty()) {
            dest_filename = dest_path.substr(slash_pos + 1);
        }
        try {
            folder_id = ensure_folder_path(folder_path);
        } catch (...) {}
    } else {
        if (dest_filename.empty()) {
            dest_filename = dest_path;
        }
    }
    if (dest_filename.empty()) {
        dest_filename = "copied_file";
    }

    // In Google Drive v3, when copying a file from another user, parents MUST be explicitly specified,
    // otherwise Drive tries to place it in the original owner's parent folder, resulting in 403 Forbidden.
    // If no parent was specified, place it in "root" (the destination user's My Drive).
    if (folder_id.empty()) {
        folder_id = "root";
    }

    std::string url = "https://www.googleapis.com/drive/v3/files/" + clean_source_id + "/copy?supportsAllDrives=true";
    std::string body = "{\"name\":\"" + dest_filename + "\",\"parents\":[\"" + folder_id + "\"]}";

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };

    auto resp = http_.post(url, body, headers);
    return (resp.status_code == 200 || resp.status_code == 201);
}

void GoogleDriveProvider::remove_share(const std::string& file_id, const std::string& permission_id) {
    std::string clean_id = sanitize_gdrive_id(file_id);
    std::string url = "https://www.googleapis.com/drive/v3/files/" + clean_id +
                      "/permissions/" + permission_id + "?supportsAllDrives=true";
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_
    };
    http_.del(url, headers); // Best-effort cleanup, ignore errors
}

} // namespace utp
