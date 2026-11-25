#include "webdav_provider.hpp"
#include "utp/security_utils.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace utp {

namespace {

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string url_encode_path(const std::string& path) {
    std::string encoded;
    std::stringstream ss(path);
    std::string segment;
    bool first = true;
    while (std::getline(ss, segment, '/')) {
        if (!first) encoded += "/";
        first = false;
        encoded += utp::security::url_encode(segment);
    }
    return encoded;
}

static std::string extract_xml_tag(const std::string& xml, const std::string& tag) {
    size_t start = xml.find("<" + tag);
    if (start == std::string::npos) {
        start = xml.find("<d:" + tag);
    }
    if (start == std::string::npos) return "";

    size_t close_bracket = xml.find('>', start);
    if (close_bracket == std::string::npos) return "";

    size_t end = xml.find("</" + tag, close_bracket);
    if (end == std::string::npos) {
        end = xml.find("</d:" + tag, close_bracket);
    }
    if (end == std::string::npos) return "";

    return xml.substr(close_bracket + 1, end - close_bracket - 1);
}

} // namespace

WebDAVProvider::WebDAVProvider(const std::string& base_url, 
                               const std::string& username, 
                               const std::string& password,
                               const std::string& name_context)
    : base_url_(base_url), username_(username), password_(password), name_context_(name_context) {
    setup_auth();
}

WebDAVProvider::~WebDAVProvider() {}

void WebDAVProvider::setup_auth() {
    if (base_url_.empty()) {
        const char* env_url = std::getenv("WEBDAV_URL");
        if (!env_url) env_url = std::getenv("NEXTCLOUD_URL");
        if (env_url && strlen(env_url) > 0) base_url_ = env_url;
    }

    if (base_url_.empty()) {
        throw TransferError(ErrorCategory::AUTH,
            "WebDAV URL not configured: set WEBDAV_URL (or NEXTCLOUD_URL) environment variable.");
    }

    // Clean trailing slash
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();

    // Refuse sending HTTP Basic Auth credentials over unencrypted HTTP except explicit local dev
    if (base_url_.rfind("https://", 0) != 0) {
        bool is_loopback = (base_url_.rfind("http://127.0.0.1", 0) == 0 ||
                            base_url_.rfind("http://localhost", 0) == 0 ||
                            base_url_.rfind("http://[::1]", 0) == 0);
        const char* allow_http = std::getenv("UTP_WEBDAV_ALLOW_HTTP");
        if (!is_loopback && !(allow_http && std::string(allow_http) == "1")) {
            throw TransferError(ErrorCategory::AUTH,
                "Refusing WebDAV Basic Auth over unencrypted HTTP. Use https:// or set UTP_WEBDAV_ALLOW_HTTP=1.");
        }
    }

    if (username_.empty()) {
        const char* env_u = std::getenv("WEBDAV_USER");
        if (!env_u) env_u = std::getenv("NEXTCLOUD_USER");
        if (env_u && strlen(env_u) > 0) username_ = env_u;
    }

    if (password_.empty()) {
        const char* env_p = std::getenv("WEBDAV_PASSWORD");
        if (!env_p) env_p = std::getenv("NEXTCLOUD_PASSWORD");
        if (env_p && strlen(env_p) > 0) password_ = env_p;
    }

    if (username_.empty() || password_.empty()) {
        throw TransferError(ErrorCategory::AUTH,
            "WebDAV credentials not configured: set WEBDAV_USER and WEBDAV_PASSWORD environment variables.");
    }

    auth_header_ = "Authorization: Basic " + base64_encode(username_ + ":" + password_);
}

std::string WebDAVProvider::build_full_url(const std::string& path) const {
    std::string clean = path;
    while (!clean.empty() && (clean.front() == '/' || clean.front() == ' ')) clean.erase(0, 1);
    while (!clean.empty() && (clean.back() == '/' || clean.back() == ' ')) clean.pop_back();
    if (clean.empty() || clean == "root" || clean == ".") return base_url_;
    return base_url_ + "/" + url_encode_path(clean);
}

ProviderCapabilities WebDAVProvider::capabilities() {
    return {
        true,  // can_stream
        true,  // supports_random_access
        false, // local_path_direct
        true,  // supports_chunking
        true,  // requires_sequential_writes
        true,  // supports_stat
        true,  // supports_list
        50 * 1024 * 1024, // max_chunk_size
        4
    };
}

FileInfo WebDAVProvider::stat(Path path) {
    std::string url = build_full_url(path);
    std::vector<std::string> headers = {
        auth_header_,
        "Depth: 0",
        "Content-Type: application/xml"
    };

    auto resp = http_.perform_custom_request("PROPFIND", url, nullptr, 0, headers);
    if (resp.status_code != 200 && resp.status_code != 207) {
        if (path.empty() || path == "root" || path == ".") {
            return {"root", 0, true, "WebDAV Root"};
        }
        throw TransferError(ErrorCategory::FATAL, "WebDAV stat failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    bool is_dir = (resp.body.find("<d:collection/>") != std::string::npos || 
                   resp.body.find("<collection/>") != std::string::npos ||
                   resp.body.find("<d:collection />") != std::string::npos);

    std::string size_str = extract_xml_tag(resp.body, "getcontentlength");
    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

    std::string name_str = extract_xml_tag(resp.body, "displayname");
    if (name_str.empty()) {
        size_t s = path.find_last_of('/');
        name_str = (s == std::string::npos) ? path : path.substr(s + 1);
    }
    if (name_str.empty()) name_str = "root";

    return {path, size, is_dir, name_str};
}

std::vector<FileInfo> WebDAVProvider::list(Path path) {
    std::vector<FileInfo> items;
    std::string url = build_full_url(path);

    std::vector<std::string> headers = {
        auth_header_,
        "Depth: 1",
        "Content-Type: application/xml"
    };

    auto resp = http_.perform_custom_request("PROPFIND", url, nullptr, 0, headers);
    if (resp.status_code != 200 && resp.status_code != 207) {
        std::cerr << "[WebDAV] list failed (" << resp.status_code << "): " << resp.body << "\n";
        return items;
    }

    size_t curr = 0;
    bool is_first = true; // The first response in Depth: 1 is the container itself
    while ((curr = resp.body.find("<d:response>", curr)) != std::string::npos ||
           (curr = resp.body.find("<response>", curr)) != std::string::npos) {
        size_t end = resp.body.find("</d:response>", curr);
        if (end == std::string::npos) end = resp.body.find("</response>", curr);
        if (end == std::string::npos) break;

        std::string block = resp.body.substr(curr, end - curr);
        curr = end + 1;

        if (is_first) {
            is_first = false;
            continue; // Skip root container
        }

        std::string href = extract_xml_tag(block, "href");
        while (!href.empty() && href.back() == '/') href.pop_back();

        std::string item_name = extract_xml_tag(block, "displayname");
        if (item_name.empty()) {
            size_t slash = href.find_last_of('/');
            if (slash != std::string::npos) item_name = href.substr(slash + 1);
            else item_name = href;
        }

        bool is_dir = (block.find("<d:collection/>") != std::string::npos || 
                       block.find("<collection/>") != std::string::npos ||
                       block.find("<d:collection />") != std::string::npos);

        std::string size_str = extract_xml_tag(block, "getcontentlength");
        uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

        std::string item_path = path.empty() ? item_name : (path + "/" + item_name);
        items.push_back({item_path, size, is_dir, item_name});
    }

    return items;
}

ReadStream WebDAVProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "webdav_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, 0, {}};
    return {id};
}

size_t WebDAVProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) throw TransferError(ErrorCategory::FATAL, "Invalid WebDAV read session");
        s = it->second;
    }

    std::string url = build_full_url(s.path);
    uint64_t end_byte = offset + count - 1;

    std::vector<std::string> headers = {
        auth_header_,
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };

    auto resp = http_.get(url, headers);
    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0;
        throw TransferError(ErrorCategory::RETRYABLE, "WebDAV read failed: " + resp.body, resp.status_code);
    }

    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count;
    std::memcpy(buffer, resp.body.data(), bytes_read);
    return bytes_read;
}

WriteSession WebDAVProvider::openWrite(Path path, uint64_t size, const std::string& resume_token) {
    (void)resume_token; // WebDAV has no upload-session concept; temp-file staging applies

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "webdav_write_" + std::to_string(next_id_++);
    Session s;
    s.id = id;
    s.path = path;
    s.total_size = size;
    s.buffer.reserve(size > 0 && size < 100 * 1024 * 1024 ? size : 0);
    sessions_[id] = std::move(s);
    return {id};
}

void WebDAVProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session.id);
    if (it == sessions_.end()) throw TransferError(ErrorCategory::FATAL, "Invalid WebDAV write session");

    if (it->second.buffer.size() < offset + size) {
        it->second.buffer.resize(offset + size);
    }
    std::memcpy(it->second.buffer.data() + offset, data, size);
}

void WebDAVProvider::commit(WriteSession session) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) return;
        s = std::move(it->second);
        sessions_.erase(it);
    }

    std::string url = build_full_url(s.path);
    std::vector<std::string> headers = {
        auth_header_,
        "Content-Type: application/octet-stream",
        "Content-Length: " + std::to_string(s.buffer.size())
    };

    auto resp = http_.put(url, s.buffer.data(), s.buffer.size(), headers);
    if (resp.status_code != 200 && resp.status_code != 201 && resp.status_code != 204) {
        throw TransferError(ErrorCategory::FATAL, "WebDAV commit failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }
}

void WebDAVProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

bool WebDAVProvider::copy_shared_file(const std::string& source_file_id, const std::string& dest_path, const std::string& filename) {
    std::string src_url = build_full_url(source_file_id);
    std::string d_p = dest_path;
    if (!filename.empty() && d_p.find(filename) == std::string::npos) {
        d_p = d_p.empty() ? filename : (d_p + "/" + filename);
    }
    std::string dest_url = build_full_url(d_p);

    std::vector<std::string> headers = {
        auth_header_,
        "Destination: " + dest_url,
        "Overwrite: T"
    };

    auto resp = http_.perform_custom_request("COPY", src_url, nullptr, 0, headers);
    return (resp.status_code == 201 || resp.status_code == 204 || resp.status_code == 200);
}

} // namespace utp
