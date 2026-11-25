#include "http_provider.hpp"
#include "utp/security_utils.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace utp {

namespace {

static std::string url_decode(const std::string& str) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < static_cast<int>(str.length()); i++) {
        if (str[i] == '%') {
            if (i + 2 < static_cast<int>(str.length())) {
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> ii) {
                    ch = static_cast<char>(ii);
                    ret += ch;
                    i += 2;
                } else {
                    ret += str[i];
                }
            } else {
                ret += str[i];
            }
        } else if (str[i] == '+') {
            ret += ' ';
        } else {
            ret += str[i];
        }
    }
    return ret;
}

static std::string clean_filename(std::string name) {
    while (!name.empty() && (name.front() == '"' || name.front() == '\'' || name.front() == ' ' || name.front() == '\t')) {
        name.erase(0, 1);
    }
    while (!name.empty() && (name.back() == '"' || name.back() == '\'' || name.back() == ';' || name.back() == ' ' || name.back() == '\t' || name.back() == '\r' || name.back() == '\n')) {
        name.pop_back();
    }
    // Remove query string if accidentally left
    size_t q = name.find('?');
    if (q != std::string::npos) name = name.substr(0, q);
    size_t h = name.find('#');
    if (h != std::string::npos) name = name.substr(0, h);
    return name;
}

} // namespace

HttpProvider::HttpProvider(const std::string& base_url, const std::string& auth_context, bool allow_private_hosts)
    : base_url_(base_url), auth_context_(auth_context), allow_private_hosts_(allow_private_hosts) {
    if (!allow_private_hosts_) {
        const char* env_allow = std::getenv("UTP_ALLOW_PRIVATE_HTTP");
        if (env_allow && std::string(env_allow) == "1") {
            allow_private_hosts_ = true;
        }
    }
}

ProviderCapabilities HttpProvider::capabilities() {
    return {
        .ranged_read = true,
        .resumable_upload = false,
        .multipart_upload = false,
        .server_side_copy = false,
        .checksum = false,
        .metadata = true,
        .directory_support = false,
        .max_chunk_size = 64ULL * 1024 * 1024,
        .max_parallel_requests = 8
    };
}

std::string HttpProvider::resolve_url(const std::string& path) const {
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        return path;
    }
    if (!base_url_.empty()) {
        if (base_url_.back() == '/' || (!path.empty() && path.front() == '/')) {
            return base_url_ + path;
        }
        return base_url_ + "/" + path;
    }
    return "https://" + path;
}

std::string HttpProvider::extract_filename(const std::map<std::string, std::string>& headers, const std::string& url) {
    // 1. Try Content-Disposition header
    auto it = headers.find("content-disposition");
    if (it != headers.end() && !it->second.empty()) {
        const std::string& cd = it->second;
        // Check for RFC 5987 / 6266 filename*=UTF-8''...
        size_t star_pos = cd.find("filename*=");
        if (star_pos != std::string::npos) {
            std::string val = cd.substr(star_pos + 10);
            size_t semi = val.find(';');
            if (semi != std::string::npos) val = val.substr(0, semi);
            size_t quotes = val.find("''");
            if (quotes != std::string::npos) val = val.substr(quotes + 2);
            val = clean_filename(url_decode(val));
            if (!val.empty()) return val;
        }

        // Check for standard filename="..."
        size_t fn_pos = cd.find("filename=");
        if (fn_pos != std::string::npos) {
            std::string val = cd.substr(fn_pos + 9);
            size_t semi = val.find(';');
            if (semi != std::string::npos) val = val.substr(0, semi);
            val = clean_filename(val);
            if (!val.empty()) return val;
        }
    }

    // 2. Parse from URL path
    std::string parsed = url;
    size_t q = parsed.find('?');
    if (q != std::string::npos) parsed = parsed.substr(0, q);
    size_t h = parsed.find('#');
    if (h != std::string::npos) parsed = parsed.substr(0, h);
    while (!parsed.empty() && parsed.back() == '/') parsed.pop_back();

    size_t slash = parsed.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < parsed.length()) {
        std::string fname = clean_filename(url_decode(parsed.substr(slash + 1)));
        if (!fname.empty() && fname.find(':') == std::string::npos) {
            return fname;
        }
    }

    return "downloaded_stream.bin";
}

FileInfo HttpProvider::stat(Path path) {
    std::string url = resolve_url(path);
    std::string ssrf_err;
    if (!utp::security::is_safe_public_http_url(url, allow_private_hosts_, &ssrf_err)) {
        throw TransferError(ErrorCategory::FATAL, "Blocked HTTP source URL (" + ssrf_err + ")");
    }
    std::vector<std::string> headers;
    if (!auth_context_.empty()) {
        headers.push_back("Authorization: Bearer " + auth_context_);
    }

    HttpResponse resp;
    bool head_ok = false;
    try {
        resp = http_.head(url, headers);
        if (resp.status_code >= 200 && resp.status_code < 400) {
            head_ok = true;
        }
    } catch (...) {
        head_ok = false;
    }

    uint64_t size = 0;
    std::string filename = "";

    if (head_ok) {
        auto it = resp.headers.find("content-length");
        if (it != resp.headers.end() && !it->second.empty()) {
            try {
                size = std::stoull(it->second);
            } catch (...) {}
        }
        filename = extract_filename(resp.headers, url);
    }

    // Fallback: If HEAD failed or gave 0 bytes (many CDNs / signed links omit or forbid HEAD)
    if (!head_ok || size == 0) {
        std::vector<std::string> range_headers = headers;
        range_headers.push_back("Range: bytes=0-0");
        try {
            resp = http_.get(url, range_headers);
            if (resp.status_code == 206) {
                // Parse Content-Range: bytes 0-0/1234567
                auto crit = resp.headers.find("content-range");
                if (crit != resp.headers.end()) {
                    size_t slash = crit->second.find('/');
                    if (slash != std::string::npos) {
                        std::string total_str = crit->second.substr(slash + 1);
                        if (total_str != "*") {
                            try { size = std::stoull(total_str); } catch (...) {}
                        }
                    }
                }
                if (filename.empty()) {
                    filename = extract_filename(resp.headers, url);
                }
            } else if (resp.status_code == 200) {
                auto it = resp.headers.find("content-length");
                if (it != resp.headers.end() && !it->second.empty()) {
                    try { size = std::stoull(it->second); } catch (...) {}
                } else {
                    size = resp.body.size();
                }
                if (filename.empty()) {
                    filename = extract_filename(resp.headers, url);
                }
            } else if (resp.status_code == 401 || resp.status_code == 403) {
                throw TransferError(ErrorCategory::AUTH, "HTTP source access denied (" + std::to_string(resp.status_code) + ")", resp.status_code);
            } else {
                throw TransferError(ErrorCategory::FATAL, "HTTP source unreachable (" + std::to_string(resp.status_code) + ")", resp.status_code);
            }
        } catch (const TransferError&) {
            throw;
        } catch (const std::exception& e) {
            throw TransferError(ErrorCategory::FATAL, std::string("HTTP connection error: ") + e.what());
        }
    }

    if (filename.empty()) {
        filename = extract_filename(resp.headers, url);
    }

    return {url, size, false, filename};
}

std::vector<FileInfo> HttpProvider::list(Path path) {
    try {
        return {stat(path)};
    } catch (...) {
        return {};
    }
}

ReadStream HttpProvider::openRead(Path path) {
    std::string full_url = resolve_url(path);
    std::string ssrf_err;
    if (!utp::security::is_safe_public_http_url(full_url, allow_private_hosts_, &ssrf_err)) {
        throw TransferError(ErrorCategory::FATAL, "Blocked HTTP source URL (" + ssrf_err + ")");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "http_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, full_url, 0};
    return {id};
}

WriteSession HttpProvider::openWrite(Path /*path*/, uint64_t /*size*/, const std::string& /*resume_token*/) {
    throw TransferError(ErrorCategory::FATAL, "HttpProvider is a read-only stream source; writing is not supported.");
}

size_t HttpProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid HTTP read session: " + session.id);
        }
        s = it->second;
    }

    uint64_t end_byte = offset + count - 1;
    std::vector<std::string> headers;
    if (!auth_context_.empty()) {
        headers.push_back("Authorization: Bearer " + auth_context_);
    }
    headers.push_back("Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte));

    auto resp = http_.get(s.url, headers);

    if (resp.status_code == 206) {
        size_t bytes_read = resp.body.size();
        if (bytes_read > count) bytes_read = count;
        std::memcpy(buffer, resp.body.data(), bytes_read);
        return bytes_read;
    } else if (resp.status_code == 416) {
        // Range Not Satisfiable (EOF reached)
        return 0;
    } else if (resp.status_code == 200) {
        // Server ignored Range header and sent from byte 0
        if (offset == 0) {
            size_t bytes_read = std::min(resp.body.size(), count);
            std::memcpy(buffer, resp.body.data(), bytes_read);
            return bytes_read;
        }
        throw TransferError(ErrorCategory::FATAL, "Remote web server does not support HTTP Range requests (returned 200 instead of 206 for byte offset)");
    } else if (resp.status_code == 401 || resp.status_code == 403) {
        throw TransferError(ErrorCategory::AUTH, "HTTP authorization failed (" + std::to_string(resp.status_code) + ")", resp.status_code);
    } else if (resp.status_code >= 500) {
        throw TransferError(ErrorCategory::RETRYABLE, "Remote server error (" + std::to_string(resp.status_code) + ")", resp.status_code);
    } else {
        throw TransferError(ErrorCategory::RETRYABLE, "HTTP read error (" + std::to_string(resp.status_code) + "): " + resp.body, resp.status_code);
    }
}

void HttpProvider::write(WriteSession /*session*/, uint64_t /*offset*/, const uint8_t* /*data*/, size_t /*size*/) {
    throw TransferError(ErrorCategory::FATAL, "HttpProvider is read-only.");
}

void HttpProvider::commit(WriteSession /*session*/) {
    throw TransferError(ErrorCategory::FATAL, "HttpProvider is read-only.");
}

void HttpProvider::abort(WriteSession /*session*/) {
    // No-op
}

} // namespace utp
