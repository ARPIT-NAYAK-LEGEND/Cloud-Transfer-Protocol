#pragma once

// Shared security, cryptography, OAuth loopback, SSRF, and sanitization helpers.
// Keep platform header order intact: <winsock2.h> must precede <windows.h>.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <aclapi.h>
#include <sddl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace utp::security {

inline std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(c);
        }
    }
    return escaped.str();
}

inline std::string url_decode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            unsigned int hex_val = 0;
            std::istringstream iss(value.substr(i + 1, 2));
            if (iss >> std::hex >> hex_val) {
                out.push_back(static_cast<char>(hex_val));
                i += 2;
                continue;
            }
        } else if (value[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(value[i]);
    }
    return out;
}

inline std::string base64url_encode(const uint8_t* data, size_t len) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    uint32_t val = 0;
    int valb = -6;
    for (size_t i = 0; i < len; ++i) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    return out;
}

inline std::vector<uint8_t> generate_random_bytes(size_t count) {
    std::vector<uint8_t> buf(count);
    if (count > 0 && RAND_bytes(buf.data(), static_cast<int>(count)) != 1) {
        throw std::runtime_error("CSPRNG failure: OpenSSL RAND_bytes failed");
    }
    return buf;
}

inline std::string generate_random_hex(size_t num_bytes) {
    auto bytes = generate_random_bytes(num_bytes);
    std::ostringstream ss;
    for (uint8_t b : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return ss.str();
}

struct PkcePair {
    std::string verifier;
    std::string challenge;
};

inline PkcePair generate_pkce_pair() {
    auto raw = generate_random_bytes(32);
    PkcePair pair;
    pair.verifier = base64url_encode(raw.data(), raw.size());
    OPENSSL_cleanse(raw.data(), raw.size());

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    if (EVP_Digest(pair.verifier.data(), pair.verifier.size(), digest, &dlen, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("EVP_Digest SHA-256 failed during PKCE generation");
    }
    pair.challenge = base64url_encode(digest, dlen);
    OPENSSL_cleanse(digest, sizeof(digest));
    return pair;
}

// Hash both inputs with SHA-256 first so length differences do not short-circuit timing.
inline bool constant_time_equals(const std::string& a, const std::string& b) {
    uint8_t da[32] = {0};
    uint8_t db[32] = {0};
    unsigned int la = 0, lb = 0;
    bool ok = (EVP_Digest(a.data(), a.size(), da, &la, EVP_sha256(), nullptr) == 1) &&
              (EVP_Digest(b.data(), b.size(), db, &lb, EVP_sha256(), nullptr) == 1) &&
              (la == 32 && lb == 32) &&
              (CRYPTO_memcmp(da, db, 32) == 0) &&
              (a.size() == b.size());
    OPENSSL_cleanse(da, sizeof(da));
    OPENSSL_cleanse(db, sizeof(db));
    return ok;
}

inline std::string hmac_sha256_hex(const std::string& key, const std::string& message) {
    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int dlen = 0;
    if (!HMAC(EVP_sha256(),
              key.data(),
              static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(message.data()),
              message.size(),
              digest,
              &dlen)) {
        throw std::runtime_error("HMAC-SHA256 computation failed");
    }
    std::ostringstream ss;
    for (unsigned int i = 0; i < dlen; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    OPENSSL_cleanse(digest, sizeof(digest));
    return ss.str();
}

// Issues a short-lived HMAC-SHA256 stream ticket bound to a specific resource key.
// Format: "<expiry_unix_sec>.<nonce_hex>.<hmac_hex>"
inline std::string create_stream_ticket(const std::string& secret,
                                        const std::string& resource_key,
                                        int ttl_seconds = 300) {
    if (secret.empty() || resource_key.empty() || ttl_seconds <= 0) {
        throw std::runtime_error("Invalid parameters for stream ticket generation");
    }
    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long exp = now_sec + ttl_seconds;
    std::string nonce = generate_random_hex(12);
    std::string payload = std::to_string(exp) + "." + nonce + ":" + resource_key;
    std::string sig = hmac_sha256_hex(secret, payload);
    return std::to_string(exp) + "." + nonce + "." + sig;
}

inline bool verify_stream_ticket(const std::string& secret,
                                 const std::string& resource_key,
                                 const std::string& ticket) {
    if (secret.empty() || resource_key.empty() || ticket.empty()) return false;
    size_t dot1 = ticket.find('.');
    if (dot1 == std::string::npos || dot1 == 0) return false;
    size_t dot2 = ticket.find('.', dot1 + 1);
    if (dot2 == std::string::npos || dot2 <= dot1 + 1 || dot2 + 1 >= ticket.size()) return false;

    std::string exp_str = ticket.substr(0, dot1);
    std::string nonce = ticket.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig = ticket.substr(dot2 + 1);

    long long exp = 0;
    try {
        size_t idx = 0;
        exp = std::stoll(exp_str, &idx);
        if (idx != exp_str.size()) return false;
    } catch (...) {
        return false;
    }

    auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (now_sec > exp) return false;

    std::string payload = exp_str + "." + nonce + ":" + resource_key;
    std::string expected_sig = hmac_sha256_hex(secret, payload);
    return constant_time_equals(sig, expected_sig);
}

inline std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b";  break;
            case '\f': o << "\\f";  break;
            case '\n': o << "\\n";  break;
            case '\r': o << "\\r";  break;
            case '\t': o << "\\t";  break;
            default:
                if (c < 0x20) {
                    o << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << static_cast<char>(c);
                }
                break;
        }
    }
    return o.str();
}

inline std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(search, pos)) != std::string::npos) {
        size_t after_key = pos + search.size();
        while (after_key < json.size() &&
               (json[after_key] == ' ' || json[after_key] == '\t' || json[after_key] == '\r' || json[after_key] == '\n')) {
            after_key++;
        }
        if (after_key >= json.size() || json[after_key] != ':') {
            pos += search.size();
            continue;
        }
        size_t val_pos = after_key + 1;
        while (val_pos < json.size() &&
               (json[val_pos] == ' ' || json[val_pos] == '\t' || json[val_pos] == '\r' || json[val_pos] == '\n')) {
            val_pos++;
        }
        if (val_pos >= json.size()) return "";

        if (json[val_pos] == '"') {
            val_pos++;
            std::string out;
            while (val_pos < json.size()) {
                char ch = json[val_pos];
                if (ch == '\\' && val_pos + 1 < json.size()) {
                    char next = json[val_pos + 1];
                    switch (next) {
                        case '"':  out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/':  out.push_back('/'); break;
                        case 'b':  out.push_back('\b'); break;
                        case 'f':  out.push_back('\f'); break;
                        case 'n':  out.push_back('\n'); break;
                        case 'r':  out.push_back('\r'); break;
                        case 't':  out.push_back('\t'); break;
                        default:   out.push_back(next); break;
                    }
                    val_pos += 2;
                } else if (ch == '"') {
                    return out;
                } else {
                    out.push_back(ch);
                    val_pos++;
                }
            }
            return out;
        } else {
            size_t end = val_pos;
            while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']' &&
                   json[end] != ' ' && json[end] != '\t' && json[end] != '\r' && json[end] != '\n') {
                end++;
            }
            return json.substr(val_pos, end - val_pos);
        }
    }
    return "";
}

inline void restrict_file_permissions(const std::string& file_path) {
#ifdef _WIN32
    _chmod(file_path.c_str(), _S_IREAD | _S_IWRITE);
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize);
        if (dwSize > 0) {
            std::vector<uint8_t> token_buf(dwSize);
            if (GetTokenInformation(hToken, TokenUser, token_buf.data(), dwSize, &dwSize)) {
                PSID user_sid = reinterpret_cast<TOKEN_USER*>(token_buf.data())->User.Sid;
                EXPLICIT_ACCESSA ea;
                ZeroMemory(&ea, sizeof(ea));
                ea.grfAccessPermissions = GENERIC_ALL;
                ea.grfAccessMode = SET_ACCESS;
                ea.grfInheritance = NO_INHERITANCE;
                ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
                ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
                ea.Trustee.ptstrName = reinterpret_cast<LPSTR>(user_sid);

                PACL new_dacl = nullptr;
                if (SetEntriesInAclA(1, &ea, nullptr, &new_dacl) == ERROR_SUCCESS && new_dacl) {
                    SetNamedSecurityInfoA(
                        const_cast<LPSTR>(file_path.c_str()),
                        SE_FILE_OBJECT,
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, new_dacl, nullptr
                    );
                    LocalFree(new_dacl);
                }
            }
        }
        CloseHandle(hToken);
    }
#else
    chmod(file_path.c_str(), S_IRUSR | S_IWUSR);
#endif
}

inline bool write_secret_file(const std::string& file_path, const std::string& content) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path p(file_path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
    }

#ifndef _WIN32
    int fd = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    fchmod(fd, S_IRUSR | S_IWUSR);
    size_t written = 0;
    while (written < content.size()) {
        ssize_t n = write(fd, content.data() + written, content.size() - written);
        if (n <= 0) {
            close(fd);
            return false;
        }
        written += static_cast<size_t>(n);
    }
    close(fd);
    return true;
#else
    {
        std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out.good()) return false;
    }
    restrict_file_permissions(file_path);
    return true;
#endif
}

inline std::string sanitize_header_filename(const std::string& raw) {
    size_t last_sep = raw.find_last_of("/\\");
    std::string base = (last_sep == std::string::npos) ? raw : raw.substr(last_sep + 1);
    std::string clean;
    clean.reserve(base.size());
    for (unsigned char c : base) {
        if (c < 0x20 || c == 0x7F || c == '"' || c == '\'' || c == ';' || c == '\\' || c == '/') {
            continue;
        }
        clean.push_back(static_cast<char>(c));
    }
    while (!clean.empty() && (clean.front() == ' ' || clean.front() == '.')) clean.erase(0, 1);
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (clean.empty() || clean == "." || clean == "..") {
        return "download.bin";
    }
    return clean;
}

inline bool is_allowed_browser_url(const std::string& url) {
    bool ok_prefix = (url.rfind("https://", 0) == 0) ||
                     (url.rfind("http://127.0.0.1:", 0) == 0) ||
                     (url.rfind("http://localhost:", 0) == 0);
    if (!ok_prefix) return false;
    for (unsigned char c : url) {
        if (c < 0x21 || c == 0x7F || c == '"' || c == '\'' || c == '`' || c == '\\') {
            return false;
        }
    }
    return true;
}

inline bool open_url_safely(const std::string& url) {
    if (!is_allowed_browser_url(url)) {
        return false;
    }
#ifdef _WIN32
    HINSTANCE res = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(res) > 32;
#elif defined(__APPLE__)
    pid_t pid = fork();
    if (pid == 0) {
        execlp("open", "open", url.c_str(), nullptr);
        _exit(1);
    }
    return pid > 0;
#else
    pid_t pid = fork();
    if (pid == 0) {
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }
        execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
        _exit(1);
    }
    return pid > 0;
#endif
}

inline std::filesystem::path resolve_within_base(const std::string& base_dir, const std::string& rel_path) {
    namespace fs = std::filesystem;
    if (base_dir.empty()) {
        throw std::runtime_error("Empty base directory");
    }

    // Reject explicit parent traversal or Windows drive-prefixed subpaths before joining.
    for (size_t i = 0; i < rel_path.size(); ++i) {
        if (rel_path[i] == '\0') {
            throw std::runtime_error("Null byte in relative path");
        }
    }
    if (rel_path.find(':') != std::string::npos) {
        throw std::runtime_error("Path contains invalid drive separator");
    }

    fs::path rel(rel_path);
    if (rel.is_absolute()) {
        throw std::runtime_error("Absolute path not allowed inside confined root");
    }
    for (const auto& part : rel) {
        if (part == "..") {
            throw std::runtime_error("Path traversal '..' rejected");
        }
    }

    fs::path base_canon = fs::weakly_canonical(fs::path(base_dir));
    fs::path combined = (base_canon / rel).lexically_normal();
    fs::path resolved = fs::weakly_canonical(combined);

    auto base_it = base_canon.begin();
    auto res_it = resolved.begin();
    for (; base_it != base_canon.end(); ++base_it, ++res_it) {
        if (res_it == resolved.end() || *res_it != *base_it) {
            throw std::runtime_error("Resolved path escapes confined root directory");
        }
    }
    return resolved;
}

inline bool is_restricted_ipv4_host_order(uint32_t ip_host, bool allow_private) {
    uint8_t a = static_cast<uint8_t>((ip_host >> 24) & 0xFF);
    uint8_t b = static_cast<uint8_t>((ip_host >> 16) & 0xFF);

    // Cloud instance metadata / link-local (169.254.0.0/16) is always restricted unless allow_private
    if (a == 169 && b == 254) return !allow_private;
    if (allow_private) return false;

    if (a == 0) return true;                                  // 0.0.0.0/8
    if (a == 10) return true;                                 // 10.0.0.0/8
    if (a == 100 && b >= 64 && b <= 127) return true;         // 100.64.0.0/10 (CGNAT)
    if (a == 127) return true;                                // 127.0.0.0/8 loopback
    if (a == 172 && b >= 16 && b <= 31) return true;          // 172.16.0.0/12
    if (a == 192 && b == 168) return true;                    // 192.168.0.0/16
    if (a >= 224) return true;                                // Multicast & reserved
    return false;
}

inline bool is_restricted_sockaddr(const struct sockaddr* sa, bool allow_private) {
    if (!sa) return true;
    if (sa->sa_family == AF_INET) {
        const auto* s4 = reinterpret_cast<const struct sockaddr_in*>(sa);
        uint32_t ip_host = ntohl(s4->sin_addr.s_addr);
        return is_restricted_ipv4_host_order(ip_host, allow_private);
    }
    if (sa->sa_family == AF_INET6) {
        const auto* s6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
        const uint8_t* b = s6->sin6_addr.s6_addr;

        // IPv4-mapped ::ffff:a.b.c.d
        bool is_v4_mapped = true;
        for (int i = 0; i < 10; ++i) {
            if (b[i] != 0) { is_v4_mapped = false; break; }
        }
        if (is_v4_mapped && b[10] == 0xFF && b[11] == 0xFF) {
            uint32_t v4 = (static_cast<uint32_t>(b[12]) << 24) |
                          (static_cast<uint32_t>(b[13]) << 16) |
                          (static_cast<uint32_t>(b[14]) << 8) |
                          static_cast<uint32_t>(b[15]);
            return is_restricted_ipv4_host_order(v4, allow_private);
        }

        // Link-local fe80::/10
        if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) return !allow_private;
        if (allow_private) return false;

        // Unspecified :: or loopback ::1
        bool all_zero = true;
        for (int i = 0; i < 15; ++i) {
            if (b[i] != 0) { all_zero = false; break; }
        }
        if (all_zero && (b[15] == 0 || b[15] == 1)) return true;

        // Unique local fc00::/7 & multicast ff00::/8
        if ((b[0] & 0xFE) == 0xFC) return true;
        if (b[0] == 0xFF) return true;
        return false;
    }
    return true;
}

inline std::string extract_http_host(const std::string& url) {
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return "";
    size_t host_start = scheme_end + 3;
    size_t path_start = url.find_first_of("/?#", host_start);
    std::string authority = (path_start == std::string::npos)
        ? url.substr(host_start)
        : url.substr(host_start, path_start - host_start);

    // Reject embedded userinfo@ to prevent SSRF parser confusion
    if (authority.find('@') != std::string::npos) return "";

    if (!authority.empty() && authority.front() == '[') {
        size_t close = authority.find(']');
        if (close == std::string::npos) return "";
        return authority.substr(1, close - 1);
    }
    size_t colon = authority.find(':');
    return (colon == std::string::npos) ? authority : authority.substr(0, colon);
}

inline bool is_safe_public_http_url(const std::string& url, bool allow_private = false, std::string* reason = nullptr) {
    auto set_reason = [&](const std::string& r) {
        if (reason) *reason = r;
    };

    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        set_reason("Only http:// and https:// schemes are permitted");
        return false;
    }
    for (unsigned char c : url) {
        if (c < 0x20 || c == 0x7F) {
            set_reason("URL contains illegal control characters");
            return false;
        }
    }

    std::string host = extract_http_host(url);
    if (host.empty()) {
        set_reason("Malformed URL host or embedded userinfo not permitted");
        return false;
    }

    std::string host_lower = host;
    for (char& ch : host_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (!allow_private && (host_lower == "localhost" || host_lower.ends_with(".localhost") ||
                           host_lower == "metadata.google.internal")) {
        set_reason("Blocked internal/metadata hostname: " + host);
        return false;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    bool wsa_init = (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
#endif
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || !res) {
#ifdef _WIN32
        if (wsa_init) WSACleanup();
#endif
        if (allow_private) return true;
        set_reason("Could not resolve host for security validation: " + host);
        return false;
    }

    bool safe = true;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (is_restricted_sockaddr(p->ai_addr, allow_private)) {
            safe = false;
            set_reason("Blocked private, loopback, or cloud metadata IP target for host: " + host);
            break;
        }
    }
    freeaddrinfo(res);
#ifdef _WIN32
    if (wsa_init) WSACleanup();
#endif
    return safe;
}

inline std::string get_query_param(const std::string& query, const std::string& key) {
    size_t start = 0;
    while (start < query.size()) {
        size_t amp = query.find('&', start);
        if (amp == std::string::npos) amp = query.size();
        std::string pair = query.substr(start, amp - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key) {
            return url_decode(pair.substr(eq + 1));
        }
        start = amp + 1;
    }
    return "";
}

inline std::string capture_oauth_code_loopback(
    int port,
    const std::string& expected_state,
    const std::string& provider_title,
    int timeout_seconds = 180
) {
    std::string captured_code;
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return "";
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        WSACleanup();
        return "";
    }
#else
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return "";
#endif

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    DWORD tv_ms = static_cast<DWORD>(timeout_seconds * 1000);
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv_ms), sizeof(tv_ms));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct timeval tv{};
    tv.tv_sec = timeout_seconds;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    // Bind strictly to loopback 127.0.0.1 so remote LAN hosts cannot inject auth codes
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(server_fd, 4) < 0) {
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return "";
    }

    for (int attempt = 0; attempt < 10 && captured_code.empty(); ++attempt) {
        struct sockaddr_in client_addr{};
#ifdef _WIN32
        int addrlen = sizeof(client_addr);
        SOCKET client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
        if (client_fd == INVALID_SOCKET) break;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv_ms), sizeof(tv_ms));
#else
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen);
        if (client_fd < 0) break;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        char buffer[8192] = {0};
        int received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        std::string request = (received > 0) ? std::string(buffer, static_cast<size_t>(received)) : "";

        // Parse only the HTTP request target on line 1: "GET /?code=...&state=... HTTP/1.1"
        size_t line_end = request.find("\r\n");
        std::string req_line = (line_end == std::string::npos) ? request : request.substr(0, line_end);
        std::string response;

        if (req_line.rfind("GET ", 0) == 0) {
            size_t sp1 = 4;
            size_t sp2 = req_line.find(' ', sp1);
            std::string target = (sp2 == std::string::npos) ? "" : req_line.substr(sp1, sp2 - sp1);

            if (target == "/favicon.ico") {
                response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
                send(client_fd, response.c_str(), static_cast<int>(response.length()), 0);
#ifdef _WIN32
                closesocket(client_fd);
#else
                close(client_fd);
#endif
                continue;
            }

            size_t q_pos = target.find('?');
            std::string query = (q_pos == std::string::npos) ? "" : target.substr(q_pos + 1);
            std::string state = get_query_param(query, "state");
            std::string code = get_query_param(query, "code");
            std::string oauth_err = get_query_param(query, "error");

            if (!oauth_err.empty()) {
                response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                           "<html><body><h2>Authentication cancelled or denied.</h2></body></html>";
                send(client_fd, response.c_str(), static_cast<int>(response.length()), 0);
#ifdef _WIN32
                closesocket(client_fd);
#else
                close(client_fd);
#endif
                break;
            }

            if (code.empty() || expected_state.empty() || !constant_time_equals(state, expected_state)) {
                response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                           "<html><body><h2>Invalid or missing OAuth state parameter.</h2></body></html>";
                send(client_fd, response.c_str(), static_cast<int>(response.length()), 0);
            } else {
                captured_code = code;
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                           "<html><body style='font-family:sans-serif;text-align:center;padding:50px;background:#0d1117;color:#c9d1d9;'>"
                           "<h2 style='color:#34d399;'>" + provider_title + " Authentication Successful!</h2>"
                           "<p>You can close this browser tab and return to UTP.</p></body></html>";
                send(client_fd, response.c_str(), static_cast<int>(response.length()), 0);
            }
        } else {
            response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
            send(client_fd, response.c_str(), static_cast<int>(response.length()), 0);
        }

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
    return captured_code;
}

} // namespace utp::security
