#include "icloud_provider.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

namespace utp {

namespace {

static std::string trim_str(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
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
               json[end] != ' ' && json[end] != '\r' && json[end] != '\n') {
            end++;
        }
        return json.substr(pos, end - pos);
    }
}

static std::string normalize_cloud_path(const std::string& path) {
    std::string p = path;
    while (!p.empty() && (p.front() == '/' || p.front() == '\\' || p.front() == ' ')) p.erase(0, 1);
    while (!p.empty() && (p.back() == '/' || p.back() == '\\' || p.back() == ' ')) p.pop_back();
    if (p == "." || p == "root") return "";
    return p;
}

} // namespace

bool ICloudProvider::is_local_bridge_available() {
    std::string p = get_local_bridge_path();
    return !p.empty();
}

std::string ICloudProvider::get_local_bridge_path() {
    // Check explicit override
    const char* env_custom = std::getenv("ICLOUD_DRIVE_PATH");
    if (!env_custom) env_custom = std::getenv("ICLOUD_LOCAL_PATH");
    if (env_custom && fs::exists(env_custom) && fs::is_directory(env_custom)) {
        return env_custom;
    }

#ifdef _WIN32
    const char* user_prof = std::getenv("USERPROFILE");
    if (user_prof) {
        fs::path p1 = fs::path(user_prof) / "iCloudDrive";
        if (fs::exists(p1) && fs::is_directory(p1)) return p1.string();

        fs::path p2 = fs::path(user_prof) / "Apple" / "iCloudDrive";
        if (fs::exists(p2) && fs::is_directory(p2)) return p2.string();
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path p = fs::path(home) / "Library" / "Mobile Documents" / "com~apple~CloudDocs";
        if (fs::exists(p) && fs::is_directory(p)) return p.string();
    }
#endif
    return "";
}

ICloudProvider::ICloudProvider(const std::string& apple_id, 
                               const std::string& auth_token,
                               const std::string& name_context)
    : apple_id_(apple_id), auth_token_(auth_token), name_context_(name_context) {
    init_session();
}

ICloudProvider::~ICloudProvider() {}

std::string ICloudProvider::get_token_file_path() const {
    if (name_context_.empty() || name_context_ == "default") {
        return ".utp_icloud_token.json";
    }
    return ".utp_icloud_" + name_context_ + "_token.json";
}

void ICloudProvider::load_token_file() {
    std::string tf = get_token_file_path();
    std::ifstream f(tf);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string aid = extract_json_value(content, "apple_id");
    std::string tok = extract_json_value(content, "token");
    std::string ds = extract_json_value(content, "dsid");
    std::string url = extract_json_value(content, "docws_url");
    std::string mode = extract_json_value(content, "mode");
    std::string lp = extract_json_value(content, "local_path");

    if (!aid.empty() && apple_id_.empty()) apple_id_ = aid;
    if (!tok.empty() && auth_token_.empty()) auth_token_ = tok;
    if (!ds.empty()) dsid_ = ds;
    if (!url.empty()) docws_url_ = url;
    if (mode == "local" || (!lp.empty() && fs::exists(lp))) {
        use_local_bridge_ = true;
        local_root_path_ = lp.empty() ? get_local_bridge_path() : lp;
    }
}

void ICloudProvider::save_token_file(const std::string& apple_id, const std::string& token, const std::string& dsid, const std::string& docws_url) {
    std::string tf = get_token_file_path();
    std::ofstream f(tf);
    if (!f.is_open()) return;

    f << "{\n";
    f << "  \"apple_id\": \"" << apple_id << "\",\n";
    f << "  \"token\": \"" << token << "\",\n";
    f << "  \"dsid\": \"" << dsid << "\",\n";
    f << "  \"docws_url\": \"" << docws_url << "\",\n";
    f << "  \"mode\": \"" << (use_local_bridge_ ? "local" : "web") << "\",\n";
    if (use_local_bridge_) {
        std::string escaped_path = local_root_path_;
        for (size_t i = 0; i < escaped_path.length(); ++i) {
            if (escaped_path[i] == '\\') {
                escaped_path.insert(i, "\\");
                i++;
            }
        }
        f << "  \"local_path\": \"" << escaped_path << "\"\n";
    } else {
        f << "  \"local_path\": \"\"\n";
    }
    f << "}\n";
}

void ICloudProvider::init_session() {
    load_token_file();

    // Check environment variables as well
    if (apple_id_.empty()) {
        const char* env_aid = std::getenv("ICLOUD_APPLE_ID");
        if (env_aid) apple_id_ = env_aid;
    }
    if (auth_token_.empty()) {
        const char* env_tok = std::getenv("ICLOUD_AUTH_TOKEN");
        if (!env_tok) env_tok = std::getenv("ICLOUD_DSWEB_TOKEN");
        if (env_tok) auth_token_ = env_tok;
    }

    // Determine mode:
    // If local bridge is available and user explicitly chose local context or no auth_token exists
    if ((name_context_ == "local" || name_context_ == "bridge" || auth_token_.empty()) && is_local_bridge_available()) {
        use_local_bridge_ = true;
        local_root_path_ = get_local_bridge_path();
        if (apple_id_.empty()) apple_id_ = "Local iCloud Drive";
        return;
    }

    if (!auth_token_.empty()) {
        cookies_ = "dsWebAuthToken=" + auth_token_;
        if (docws_url_.empty() || dsid_.empty()) {
            authenticate_web(apple_id_, auth_token_);
        }
    } else if (is_local_bridge_available()) {
        use_local_bridge_ = true;
        local_root_path_ = get_local_bridge_path();
        if (apple_id_.empty()) apple_id_ = "Local iCloud Drive";
    }
}

bool ICloudProvider::authenticate_web(const std::string& apple_id, const std::string& session_token) {
    if (session_token.empty()) return false;

    apple_id_ = apple_id;
    auth_token_ = session_token;
    cookies_ = "dsWebAuthToken=" + auth_token_;

    std::vector<std::string> headers = {
        "Cookie: " + cookies_,
        "Accept: application/json",
        "Content-Type: application/json",
        "Origin: https://www.icloud.com",
        "Referer: https://www.icloud.com/"
    };

    try {
        auto resp = http_.post("https://setup.icloud.com/setup/ws/1/validate", "{}", headers);
        if (resp.status_code == 200) {
            dsid_ = extract_json_value(resp.body, "dsid");
            // Find docws url
            size_t docws_pos = resp.body.find("\"docws\"");
            if (docws_pos != std::string::npos) {
                docws_url_ = extract_json_value(resp.body.substr(docws_pos), "url");
            }
            if (docws_url_.empty()) {
                docws_url_ = "https://p58-docws.icloud.com:443";
            }
            save_token_file(apple_id_, auth_token_, dsid_, docws_url_);
            return true;
        }
    } catch (...) {}

    // Fallback default docws endpoint if validate is blocked
    if (docws_url_.empty()) docws_url_ = "https://p58-docws.icloud.com:443";
    return true;
}

std::string ICloudProvider::get_user_identity() const {
    if (use_local_bridge_) return "iCloud Drive (Local Sync)";
    return apple_id_.empty() ? "iCloud Account" : apple_id_;
}

ProviderCapabilities ICloudProvider::capabilities() {
    return {
        true,                // can_stream
        true,                // supports_random_access
        use_local_bridge_,   // local_path_direct (fast path if local)
        true,                // supports_chunking
        true,                // requires_sequential_writes
        true,                // supports_stat
        true,                // supports_list
        50 * 1024 * 1024,    // max_chunk_size
        4                    // recommended_concurrency
    };
}

FileInfo ICloudProvider::stat(Path path) {
    std::string clean = normalize_cloud_path(path);

    // ── Local Bridge Fast Path ──
    if (use_local_bridge_) {
        fs::path full_p = fs::path(local_root_path_) / clean;
        if (!fs::exists(full_p)) {
            if (clean.empty()) return {"root", 0, true, "iCloud Drive"};
            throw TransferError(ErrorCategory::FATAL, "iCloud file not found: " + path);
        }
        bool is_d = fs::is_directory(full_p);
        uint64_t sz = is_d ? 0 : fs::file_size(full_p);
        std::string n = full_p.filename().string();
        if (n.empty()) n = "iCloud Drive";
        return {clean, sz, is_d, n};
    }

    // ── Web docws API Path ──
    if (clean.empty()) {
        return {"root", 0, true, "iCloud Drive"};
    }

    // List parent to find item
    size_t last_slash = clean.find_last_of('/');
    std::string parent = (last_slash == std::string::npos) ? "" : clean.substr(0, last_slash);
    std::string item_name = (last_slash == std::string::npos) ? clean : clean.substr(last_slash + 1);

    auto items = list(parent);
    for (const auto& it : items) {
        if (it.name == item_name || it.path == clean) {
            return it;
        }
    }

    throw TransferError(ErrorCategory::FATAL, "iCloud item not found: " + path);
}

std::vector<FileInfo> ICloudProvider::list(Path path) {
    std::string clean = normalize_cloud_path(path);
    std::vector<FileInfo> results;

    // ── Local Bridge Fast Path ──
    if (use_local_bridge_) {
        fs::path full_p = fs::path(local_root_path_) / clean;
        if (!fs::exists(full_p) || !fs::is_directory(full_p)) {
            return results;
        }

        try {
            for (const auto& entry : fs::directory_iterator(full_p)) {
                bool is_d = entry.is_directory();
                uint64_t sz = 0;
                try {
                    if (!is_d) sz = entry.file_size();
                } catch (...) {}
                std::string filename = entry.path().filename().string();
                std::string item_rel = clean.empty() ? filename : (clean + "/" + filename);
                results.push_back({item_rel, sz, is_d, filename});
            }
        } catch (const std::exception& e) {
            std::cerr << "[iCloud] list directory error: " << e.what() << "\n";
        }
        return results;
    }

    // ── Web docws API Path ──
    if (docws_url_.empty() || dsid_.empty()) {
        return results;
    }

    std::string url;
    std::string body;
    if (clean.empty()) {
        url = docws_url_ + "/ws/" + dsid_ + "/lookup/root";
        body = "{\"requestData\":{}}";
    } else {
        std::string drivewsid = resolve_drivewsid(clean);
        url = docws_url_ + "/ws/" + dsid_ + "/lookup";
        body = "{\"requestData\":{\"items\":[{\"drivewsid\":\"" + drivewsid + "\"}]}}";
    }

    std::vector<std::string> headers = {
        "Cookie: " + cookies_,
        "Accept: application/json",
        "Content-Type: application/json",
        "Origin: https://www.icloud.com",
        "Referer: https://www.icloud.com/"
    };

    try {
        auto resp = http_.post(url, body, headers);
        if (resp.status_code == 200) {
            // Parse items array
            size_t items_pos = resp.body.find("\"items\":[");
            if (items_pos != std::string::npos) {
                size_t curr = items_pos + 8;
                while ((curr = resp.body.find('{', curr)) != std::string::npos) {
                    size_t end_obj = resp.body.find('}', curr);
                    if (end_obj == std::string::npos) break;
                    std::string obj = resp.body.substr(curr, end_obj - curr + 1);
                    curr = end_obj + 1;

                    std::string name = extract_json_value(obj, "name");
                    std::string type = extract_json_value(obj, "type");
                    std::string size_str = extract_json_value(obj, "size");
                    if (name.empty()) continue;

                    bool is_dir = (type == "FOLDER");
                    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);
                    std::string item_path = clean.empty() ? name : (clean + "/" + name);
                    results.push_back({item_path, size, is_dir, name});
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[iCloud] Web API list error: " << e.what() << "\n";
    }

    return results;
}

std::string ICloudProvider::resolve_drivewsid(const std::string& path) {
    if (path.empty() || path == "root") return "FOLDER::com.apple.CloudDocs::root";

    // Navigate segment by segment
    std::stringstream ss(path);
    std::string seg;
    std::string curr_id = "FOLDER::com.apple.CloudDocs::root";

    while (std::getline(ss, seg, '/')) {
        if (seg.empty()) continue;
        std::string url = docws_url_ + "/ws/" + dsid_ + "/lookup";
        std::string body = "{\"requestData\":{\"items\":[{\"drivewsid\":\"" + curr_id + "\"}]}}";
        std::vector<std::string> headers = {
            "Cookie: " + cookies_,
            "Accept: application/json",
            "Content-Type: application/json"
        };
        auto resp = http_.post(url, body, headers);
        if (resp.status_code != 200) break;

        size_t n_pos = resp.body.find("\"name\":\"" + seg + "\"");
        if (n_pos == std::string::npos) break;
        size_t obj_start = resp.body.rfind('{', n_pos);
        if (obj_start == std::string::npos) break;
        std::string sub = resp.body.substr(obj_start);
        std::string found_id = extract_json_value(sub, "drivewsid");
        if (!found_id.empty()) {
            curr_id = found_id;
        }
    }
    return curr_id;
}

ReadStream ICloudProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "icloud_read_" + std::to_string(next_id_++);
    std::string clean = normalize_cloud_path(path);

    Session s;
    s.id = id;
    s.path = clean;

    if (!use_local_bridge_) {
        // Request download token & CDN url
        std::string drivewsid = resolve_drivewsid(clean);
        std::string url = docws_url_ + "/ws/" + dsid_ + "/download/token";
        std::string body = "{\"document_id\":\"" + drivewsid + "\"}";
        std::vector<std::string> headers = {
            "Cookie: " + cookies_,
            "Accept: application/json",
            "Content-Type: application/json"
        };
        auto resp = http_.post(url, body, headers);
        if (resp.status_code == 200) {
            size_t u_pos = resp.body.find("\"url\":\"");
            if (u_pos != std::string::npos) {
                s.download_url = extract_json_value(resp.body.substr(u_pos), "url");
            }
        }
    }

    sessions_[id] = std::move(s);
    return {id};
}

size_t ICloudProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) throw TransferError(ErrorCategory::FATAL, "Invalid iCloud read session");
        s = it->second;
    }

    // ── Local Bridge Fast Path ──
    if (use_local_bridge_) {
        fs::path full_p = fs::path(local_root_path_) / s.path;
        std::ifstream file(full_p, std::ios::binary);
        if (!file.is_open()) {
            throw TransferError(ErrorCategory::FATAL, "Cannot open iCloud file for reading: " + s.path);
        }
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(buffer), count);
        return static_cast<size_t>(file.gcount());
    }

    // ── Web docws CDN Streaming Path ──
    if (s.download_url.empty()) {
        throw TransferError(ErrorCategory::FATAL, "iCloud download URL expired or unavailable");
    }

    uint64_t end_byte = offset + count - 1;
    std::vector<std::string> headers = {
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };

    auto resp = http_.get(s.download_url, headers);
    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0;
        throw TransferError(ErrorCategory::RETRYABLE, "iCloud stream failed (" + std::to_string(resp.status_code) + ")", resp.status_code);
    }

    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count;
    std::memcpy(buffer, resp.body.data(), bytes_read);
    return bytes_read;
}

WriteSession ICloudProvider::openWrite(Path path, uint64_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "icloud_write_" + std::to_string(next_id_++);
    std::string clean = normalize_cloud_path(path);

    Session s;
    s.id = id;
    s.path = clean;
    s.total_size = size;

    if (use_local_bridge_) {
        fs::path full_p = fs::path(local_root_path_) / clean;
        fs::create_directories(full_p.parent_path());
    } else {
        s.buffer.reserve(size > 0 && size < 50 * 1024 * 1024 ? size : 0);
    }

    sessions_[id] = std::move(s);
    return {id};
}

void ICloudProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session.id);
    if (it == sessions_.end()) throw TransferError(ErrorCategory::FATAL, "Invalid iCloud write session");

    if (use_local_bridge_) {
        fs::path full_p = fs::path(local_root_path_) / it->second.path;
        std::fstream file(full_p, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            file.open(full_p, std::ios::out | std::ios::binary);
        }
        file.seekp(offset);
        file.write(reinterpret_cast<const char*>(data), size);
        file.flush();
    } else {
        if (it->second.buffer.size() < offset + size) {
            it->second.buffer.resize(offset + size);
        }
        std::memcpy(it->second.buffer.data() + offset, data, size);
    }
}

void ICloudProvider::commit(WriteSession session) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) return;
        s = std::move(it->second);
        sessions_.erase(it);
    }

    if (use_local_bridge_) {
        // Local bridge auto-syncs to iCloud in the background
        return;
    }

    // In web mode, upload buffer to Apple CDN upload package endpoint
    std::string url = docws_url_ + "/ws/" + dsid_ + "/upload/token";
    std::string body = "{\"filename\":\"" + s.path + "\",\"size\":" + std::to_string(s.buffer.size()) + "}";
    std::vector<std::string> headers = {
        "Cookie: " + cookies_,
        "Accept: application/json",
        "Content-Type: application/json"
    };
    try {
        auto resp = http_.post(url, body, headers);
        if (resp.status_code == 200) {
            std::string upload_url = extract_json_value(resp.body, "url");
            if (!upload_url.empty()) {
                std::vector<std::string> up_headers = {
                    "Content-Type: application/octet-stream",
                    "Content-Length: " + std::to_string(s.buffer.size())
                };
                http_.put(upload_url, s.buffer.data(), s.buffer.size(), up_headers);
            }
        }
    } catch (...) {}
}

void ICloudProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

bool ICloudProvider::copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) {
    if (use_local_bridge_) {
        fs::path src = fs::path(local_root_path_) / normalize_cloud_path(source_file_id);
        std::string d_p = dest_path;
        if (!filename.empty() && d_p.find(filename) == std::string::npos) {
            d_p = d_p.empty() ? filename : (d_p + "/" + filename);
        }
        fs::path dst = fs::path(local_root_path_) / normalize_cloud_path(d_p);
        fs::create_directories(dst.parent_path());
        std::error_code ec;
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        return !ec;
    }
    return false;
}

} // namespace utp
