#include "s3_provider.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace utp {

namespace {

static std::string sha256_hex(const void* data, size_t len) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data), len, hash);
    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    return std::string(hex, SHA256_DIGEST_LENGTH * 2);
}

static std::vector<unsigned char> hmac_sha256(const void* key, size_t key_len, const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int len = SHA256_DIGEST_LENGTH;
    HMAC(EVP_sha256(), key, key_len,
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         hash, &len);
    return std::vector<unsigned char>(hash, hash + len);
}

static std::string hmac_sha256_hex(const std::vector<unsigned char>& key, const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int len = SHA256_DIGEST_LENGTH;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         hash, &len);
    char hex[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", hash[i]);
    }
    return std::string(hex, SHA256_DIGEST_LENGTH * 2);
}

static std::string get_iso8601_time(time_t t) {
    char buf[32];
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm_buf);
    return std::string(buf);
}

static std::string extract_xml_tag(const std::string& xml, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t p1 = xml.find(open);
    if (p1 == std::string::npos) return "";
    p1 += open.length();
    size_t p2 = xml.find(close, p1);
    if (p2 == std::string::npos) return "";
    return xml.substr(p1, p2 - p1);
}

} // namespace

S3Provider::S3Provider(const std::string& access_key, const std::string& secret_key, const std::string& region, const std::string& custom_endpoint) {
    access_key_ = access_key;
    secret_key_ = secret_key;

    const char* env_ak = std::getenv("AWS_ACCESS_KEY_ID");
    if (env_ak && strlen(env_ak) > 0) access_key_ = env_ak;

    const char* env_sk = std::getenv("AWS_SECRET_ACCESS_KEY");
    if (env_sk && strlen(env_sk) > 0) secret_key_ = env_sk;

    const char* env_reg = std::getenv("AWS_REGION");
    if (!env_reg) env_reg = std::getenv("AWS_DEFAULT_REGION");

    if (!region.empty()) {
        region_ = region;
    } else if (env_reg && strlen(env_reg) > 0) {
        region_ = env_reg;
    } else {
        region_ = "ap-south-1"; // Default region
    }

    if (!custom_endpoint.empty()) {
        endpoint_ = custom_endpoint;
    } else {
        endpoint_ = (region_ == "us-east-1") ? "https://s3.amazonaws.com" : ("https://s3." + region_ + ".amazonaws.com");
    }
}

S3Provider::~S3Provider() {}

S3Provider::SignedRequest S3Provider::build_signed_request(
    const std::string& method, 
    const std::string& url, 
    const uint8_t* payload, 
    size_t payload_len, 
    const std::vector<std::string>& extra_headers
) {
    // Parse URL into host, path, query
    std::string host, path, query;
    size_t proto_pos = url.find("://");
    size_t host_start = (proto_pos == std::string::npos) ? 0 : (proto_pos + 3);
    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        host = url.substr(host_start);
        path = "/";
    } else {
        host = url.substr(host_start, path_start - host_start);
        size_t query_pos = url.find('?', path_start);
        if (query_pos == std::string::npos) {
            path = url.substr(path_start);
        } else {
            path = url.substr(path_start, query_pos - path_start);
            query = url.substr(query_pos + 1);
        }
    }

    time_t now = time(nullptr);
    std::string amz_date = get_iso8601_time(now);
    std::string date_str = amz_date.substr(0, 8);

    std::string payload_hash = sha256_hex(payload ? payload : reinterpret_cast<const uint8_t*>(""), payload_len);

    // Canonical query string (sorted key-value pairs)
    std::string canonical_query = "";
    if (!query.empty()) {
        std::vector<std::string> params;
        size_t start = 0;
        while (start < query.size()) {
            size_t next = query.find('&', start);
            if (next == std::string::npos) next = query.size();
            params.push_back(query.substr(start, next - start));
            start = next + 1;
        }
        std::sort(params.begin(), params.end());
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) canonical_query += "&";
            if (params[i].find('=') == std::string::npos) {
                canonical_query += params[i] + "=";
            } else {
                canonical_query += params[i];
            }
        }
    }

    // Canonical headers
    std::map<std::string, std::string> canonical_headers_map;
    canonical_headers_map["host"] = host;
    canonical_headers_map["x-amz-content-sha256"] = payload_hash;
    canonical_headers_map["x-amz-date"] = amz_date;

    for (const auto& h : extra_headers) {
        size_t colon = h.find(':');
        if (colon != std::string::npos) {
            std::string k = h.substr(0, colon);
            std::string v = h.substr(colon + 1);
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(0, 1);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r' || v.back() == '\n')) v.pop_back();
            std::string lower_k = k;
            for (char& c : lower_k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower_k == "range" || lower_k.rfind("x-amz-", 0) == 0) {
                canonical_headers_map[lower_k] = v;
            }
        }
    }

    std::string canonical_headers_str;
    std::string signed_headers_str;
    for (auto it = canonical_headers_map.begin(); it != canonical_headers_map.end(); ++it) {
        if (it != canonical_headers_map.begin()) signed_headers_str += ";";
        signed_headers_str += it->first;
        canonical_headers_str += it->first + ":" + it->second + "\n";
    }

    std::string canonical_request = method + "\n" +
                                    path + "\n" +
                                    canonical_query + "\n" +
                                    canonical_headers_str + "\n" +
                                    signed_headers_str + "\n" +
                                    payload_hash;

    std::string string_to_sign = "AWS4-HMAC-SHA256\n" +
                                 amz_date + "\n" +
                                 date_str + "/" + region_ + "/s3/aws4_request\n" +
                                 sha256_hex(canonical_request.data(), canonical_request.size());

    std::string kSecret = "AWS4" + secret_key_;
    auto kDate = hmac_sha256(kSecret.data(), kSecret.size(), date_str);
    auto kRegion = hmac_sha256(kDate.data(), kDate.size(), region_);
    auto kService = hmac_sha256(kRegion.data(), kRegion.size(), "s3");
    auto kSigning = hmac_sha256(kService.data(), kService.size(), "aws4_request");

    std::string signature = hmac_sha256_hex(kSigning, string_to_sign);

    std::string auth_header = "Authorization: AWS4-HMAC-SHA256 Credential=" + access_key_ + "/" +
                              date_str + "/" + region_ + "/s3/aws4_request, SignedHeaders=" +
                              signed_headers_str + ", Signature=" + signature;

    std::vector<std::string> out_headers = {
        "Host: " + host,
        "x-amz-date: " + amz_date,
        "x-amz-content-sha256: " + payload_hash,
        auth_header
    };

    for (const auto& h : extra_headers) {
        out_headers.push_back(h);
    }

    return {url, out_headers};
}

ProviderCapabilities S3Provider::capabilities() {
    return {
        true,  // ranged_read
        false, // resumable_upload (S3 uses multipart)
        true,  // multipart_upload
        true,  // server_side_copy
        true,  // checksum (ETag)
        true,  // metadata
        false, // directory_support (S3 is flat)
        5ULL * 1024 * 1024 * 1024, // max_chunk_size (5GB)
        16     // max_parallel_requests
    };
}

FileInfo S3Provider::stat(Path path) {
    std::string url = endpoint_ + "/" + path;
    auto req = build_signed_request("HEAD", url, nullptr, 0, {});
    auto resp = http_.head(req.url, req.headers);

    // Auto-detect region if bucket is in a different AWS region
    if (resp.status_code == 301 || resp.status_code == 400) {
        auto it = resp.headers.find("x-amz-bucket-region");
        if (it != resp.headers.end() && !it->second.empty() && it->second != region_) {
            region_ = it->second;
            endpoint_ = (region_ == "us-east-1") ? "https://s3.amazonaws.com" : ("https://s3." + region_ + ".amazonaws.com");
            url = endpoint_ + "/" + path;
            req = build_signed_request("HEAD", url, nullptr, 0, {});
            resp = http_.head(req.url, req.headers);
        }
    }

    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "S3 stat failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::string size_str = "";
    auto it = resp.headers.find("content-length");
    if (it != resp.headers.end()) size_str = it->second;
    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);

    return {path, size, false};
}

std::vector<FileInfo> S3Provider::list(Path path) {
    std::vector<FileInfo> items;
    if (path.empty() || path == "/") {
        std::string url = endpoint_ + "/";
        auto req = build_signed_request("GET", url, nullptr, 0, {});
        auto resp = http_.get(req.url, req.headers);
        if (resp.status_code == 200) {
            size_t pos = 0;
            while (true) {
                size_t b_pos = resp.body.find("<Bucket>", pos);
                if (b_pos == std::string::npos) break;
                size_t b_end = resp.body.find("</Bucket>", b_pos);
                if (b_end == std::string::npos) break;
                std::string b_xml = resp.body.substr(b_pos, b_end - b_pos + 9);
                std::string b_name = extract_xml_tag(b_xml, "Name");
                if (!b_name.empty()) {
                    items.push_back({b_name, 0, true, b_name});
                }
                pos = b_end + 9;
            }
        }
        return items;
    }

    std::string bucket = path;
    std::string prefix = "";
    size_t slash = path.find('/');
    if (slash != std::string::npos) {
        bucket = path.substr(0, slash);
        prefix = path.substr(slash + 1);
    }

    std::string url = endpoint_ + "/" + bucket + "?list-type=2";
    if (!prefix.empty()) {
        url += "&prefix=" + prefix;
    }
    url += "&delimiter=/";

    auto req = build_signed_request("GET", url, nullptr, 0, {});
    auto resp = http_.get(req.url, req.headers);
    if (resp.status_code == 200) {
        size_t cp_pos = 0;
        while (true) {
            size_t p1 = resp.body.find("<CommonPrefixes>", cp_pos);
            if (p1 == std::string::npos) break;
            size_t p2 = resp.body.find("</CommonPrefixes>", p1);
            if (p2 == std::string::npos) break;
            std::string sub = resp.body.substr(p1, p2 - p1 + 17);
            std::string p_name = extract_xml_tag(sub, "Prefix");
            if (!p_name.empty()) {
                std::string display = p_name;
                if (!display.empty() && display.back() == '/') display.pop_back();
                size_t last_s = display.find_last_of('/');
                if (last_s != std::string::npos) display = display.substr(last_s + 1);
                items.push_back({bucket + "/" + p_name, 0, true, display});
            }
            cp_pos = p2 + 17;
        }

        size_t c_pos = 0;
        while (true) {
            size_t p1 = resp.body.find("<Contents>", c_pos);
            if (p1 == std::string::npos) break;
            size_t p2 = resp.body.find("</Contents>", p1);
            if (p2 == std::string::npos) break;
            std::string sub = resp.body.substr(p1, p2 - p1 + 11);
            std::string key = extract_xml_tag(sub, "Key");
            std::string sz = extract_xml_tag(sub, "Size");
            if (!key.empty() && key.back() != '/') {
                uint64_t size_val = sz.empty() ? 0 : std::stoull(sz);
                std::string display = key;
                size_t last_s = display.find_last_of('/');
                if (last_s != std::string::npos) display = display.substr(last_s + 1);
                items.push_back({bucket + "/" + key, size_val, false, display});
            }
            c_pos = p2 + 11;
        }
    }
    return items;
}

ReadStream S3Provider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "s3_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, "", {}};
    return {id};
}

WriteSession S3Provider::openWrite(Path path, uint64_t size, const std::string& resume_token) {
    (void)resume_token; // TODO(Phase 3): persist multipart UploadId for S3/B2/R2 resume

    std::string url = endpoint_ + "/" + path + "?uploads";
    auto req = build_signed_request("POST", url, nullptr, 0, {});
    auto resp = http_.post(req.url, "", req.headers);

    // Auto-detect region if needed
    if (resp.status_code == 301 || resp.status_code == 400) {
        auto it = resp.headers.find("x-amz-bucket-region");
        if (it != resp.headers.end() && !it->second.empty() && it->second != region_) {
            region_ = it->second;
            endpoint_ = (region_ == "us-east-1") ? "https://s3.amazonaws.com" : ("https://s3." + region_ + ".amazonaws.com");
            url = endpoint_ + "/" + path + "?uploads";
            req = build_signed_request("POST", url, nullptr, 0, {});
            resp = http_.post(req.url, "", req.headers);
        }
    }

    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "S3 CreateMultipartUpload failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::string upload_id = extract_xml_tag(resp.body, "UploadId");
    if (upload_id.empty()) {
        throw TransferError(ErrorCategory::FATAL, "S3 missing UploadId in response: " + resp.body);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "s3_write_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, upload_id, {}};
    return {id};
}

size_t S3Provider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid S3 read session");
        }
        s = sessions_[session.id];
    }

    uint64_t end_byte = offset + count - 1;
    std::string url = endpoint_ + "/" + s.path;
    std::vector<std::string> extra = {
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };

    auto req = build_signed_request("GET", url, nullptr, 0, extra);
    auto resp = http_.get(req.url, req.headers);

    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0; // EOF
        if (resp.status_code == 403) throw TransferError(ErrorCategory::AUTH, "S3 auth error: " + resp.body, resp.status_code);
        throw TransferError(ErrorCategory::RETRYABLE, "S3 read failed (" + std::to_string(resp.status_code) + "): " + resp.body, resp.status_code);
    }

    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count;
    std::memcpy(buffer, resp.body.data(), bytes_read);

    return bytes_read;
}

void S3Provider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    Session* s = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid S3 write session");
        }
        s = &sessions_[session.id];
    }

    uint32_t part_number = static_cast<uint32_t>(offset / (8ULL * 1024 * 1024)) + 1;
    std::string url = endpoint_ + "/" + s->path + "?partNumber=" + std::to_string(part_number) + "&uploadId=" + s->upload_id;

    auto req = build_signed_request("PUT", url, data, size, {});
    auto resp = http_.put(req.url, data, size, req.headers);

    if (resp.status_code != 200) {
        if (resp.status_code == 403) throw TransferError(ErrorCategory::AUTH, "S3 auth error: " + resp.body, resp.status_code);
        if (resp.status_code == 503) throw TransferError(ErrorCategory::RATE_LIMIT, "S3 slow down", resp.status_code);
        throw TransferError(ErrorCategory::RETRYABLE, "S3 UploadPart failed (" + std::to_string(resp.status_code) + "): " + resp.body, resp.status_code);
    }

    std::string etag = "";
    auto it = resp.headers.find("etag");
    if (it != resp.headers.end()) etag = it->second;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        s->etags[part_number] = etag;
    }
}

void S3Provider::commit(WriteSession session) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) return;
        s = it->second;
    }

    std::string xml = "<CompleteMultipartUpload>\n";
    for (const auto& [part_num, etag] : s.etags) {
        xml += "  <Part>\n";
        xml += "    <PartNumber>" + std::to_string(part_num) + "</PartNumber>\n";
        xml += "    <ETag>" + etag + "</ETag>\n";
        xml += "  </Part>\n";
    }
    xml += "</CompleteMultipartUpload>";

    std::string url = endpoint_ + "/" + s.path + "?uploadId=" + s.upload_id;
    auto req = build_signed_request("POST", url, reinterpret_cast<const uint8_t*>(xml.data()), xml.size(), {});
    auto resp = http_.post(req.url, xml, req.headers);

    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "S3 CompleteMultipartUpload failed (" + std::to_string(resp.status_code) + "): " + resp.body);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

void S3Provider::abort(WriteSession session) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session.id);
        if (it == sessions_.end()) return;
        s = it->second;
        sessions_.erase(it);
    }

    std::string url = endpoint_ + "/" + s.path + "?uploadId=" + s.upload_id;
    auto req = build_signed_request("DELETE", url, nullptr, 0, {});
    http_.del(req.url, req.headers);
}

} // namespace utp
