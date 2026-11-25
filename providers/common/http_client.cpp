#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace utp {

static const char* get_ca_bundle_path() {
    static std::string s_ca_path;
    static std::once_flag s_ca_once;
    std::call_once(s_ca_once, []() {
        const char* env_ca = std::getenv("CURL_CA_BUNDLE");
        if (!env_ca) env_ca = std::getenv("SSL_CERT_FILE");
        if (env_ca) {
            std::error_code ec;
            if (fs::exists(env_ca, ec)) {
                s_ca_path = env_ca;
                return;
            }
        }

#ifdef _WIN32
        char exe_path[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, exe_path, MAX_PATH)) {
            std::error_code ec;
            fs::path exe_dir = fs::path(exe_path).parent_path();
            fs::path local_bundle = exe_dir / "ca-bundle.crt";
            if (fs::exists(local_bundle, ec)) {
                s_ca_path = local_bundle.string();
                return;
            }
            fs::path local_pem = exe_dir / "cacert.pem";
            if (fs::exists(local_pem, ec)) {
                s_ca_path = local_pem.string();
                return;
            }
        }
#endif

        // Note: intentionally skip bare relative CWD "ca-bundle.crt"/"cacert.pem" to prevent CWD CA spoofing.
        const char* candidates[] = {
            "C:\\Program Files\\Git\\ucrt64\\etc\\ssl\\certs\\ca-bundle.crt",
            "C:\\Program Files\\Git\\mingw64\\etc\\ssl\\certs\\ca-bundle.crt",
            "C:\\Program Files\\Git\\usr\\ssl\\certs\\ca-bundle.crt",
            "C:\\Program Files\\Git\\mingw64\\ssl\\certs\\ca-bundle.crt",
            "C:\\msys64\\ucrt64\\etc\\ssl\\certs\\ca-bundle.crt",
            "C:\\msys64\\etc\\ssl\\certs\\ca-bundle.crt",
            "C:\\msys64\\usr\\ssl\\certs\\ca-bundle.crt",
            "/etc/ssl/certs/ca-certificates.crt",
            "/etc/pki/tls/certs/ca-bundle.crt",
            "/etc/ssl/ca-bundle.pem",
            "/etc/ssl/cert.pem"
        };

        for (const char* path : candidates) {
            std::error_code ec;
            if (fs::exists(path, ec)) {
                s_ca_path = path;
                return;
            }
        }
    });
    return s_ca_path.empty() ? nullptr : s_ca_path.c_str();
}

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realsize);
    return realsize;
}

static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t numbytes = size * nitems;
    std::string header(buffer, numbytes);
    
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    size_t pos = header.find(':');
    if (pos != std::string::npos) {
        std::string key = header.substr(0, pos);
        std::string value = header.substr(pos + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
        std::string lower_key = key;
        for (char& c : lower_key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        (*headers)[key] = value;
        (*headers)[lower_key] = value;
    }
    return numbytes;
}

static std::once_flag g_curl_init_flag;
static CURLSH* g_curl_share = nullptr;
static std::mutex g_share_mutexes[4];

static void share_lock_cb(CURL* /*handle*/, curl_lock_data data, curl_lock_access /*access*/, void* /*userptr*/) {
    int idx = 0;
    if (data == CURL_LOCK_DATA_CONNECT) idx = 1;
    else if (data == CURL_LOCK_DATA_DNS) idx = 2;
    else if (data == CURL_LOCK_DATA_SSL_SESSION) idx = 3;
    g_share_mutexes[idx].lock();
}

static void share_unlock_cb(CURL* /*handle*/, curl_lock_data data, void* /*userptr*/) {
    int idx = 0;
    if (data == CURL_LOCK_DATA_CONNECT) idx = 1;
    else if (data == CURL_LOCK_DATA_DNS) idx = 2;
    else if (data == CURL_LOCK_DATA_SSL_SESSION) idx = 3;
    g_share_mutexes[idx].unlock();
}

HttpClient::HttpClient() {
    std::call_once(g_curl_init_flag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_curl_share = curl_share_init();
        if (g_curl_share) {
            curl_share_setopt(g_curl_share, CURLSHOPT_LOCKFUNC, share_lock_cb);
            curl_share_setopt(g_curl_share, CURLSHOPT_UNLOCKFUNC, share_unlock_cb);
            curl_share_setopt(g_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
            curl_share_setopt(g_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
            curl_share_setopt(g_curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        }
    });
}

HttpClient::~HttpClient() {
    // Keep global curl resources alive for application lifetime
}

} // namespace utp // end of per-instance impl

// Per-process byte counters — updated by CURL transfer callbacks, read by CLI HUD
static std::atomic<uint64_t> s_network_dl_tick{0};
static std::atomic<uint64_t> s_network_ul_tick{0};

namespace utp { // reopen

static int XferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    uint64_t* last_bytes = static_cast<uint64_t*>(clientp);
    uint64_t last_dl = last_bytes[0];
    uint64_t last_ul = last_bytes[1];
    
    if (static_cast<uint64_t>(dlnow) > last_dl) {
        uint64_t diff = static_cast<uint64_t>(dlnow) - last_dl;
        s_network_dl_tick += diff;
        last_bytes[0] = dlnow;
    }
    
    if (static_cast<uint64_t>(ulnow) > last_ul) {
        uint64_t diff = static_cast<uint64_t>(ulnow) - last_ul;
        s_network_ul_tick += diff;
        last_bytes[1] = ulnow;
    }
    
    return 0;
}

HttpResponse HttpClient::perform_request(const std::string& url, const char* method, const uint8_t* data, size_t size, const std::vector<std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    HttpResponse response;
    uint64_t last_transfer_bytes[2] = {0, 0};
    
    struct curl_slist* chunk = nullptr;
    for (const auto& h : headers) {
        chunk = curl_slist_append(chunk, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    if (std::string(method) == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    if (data && size > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(size));
    }

    // Industrial-grade SSL/TLS configuration — verify by default.
    // Set UTP_INSECURE_SKIP_VERIFY=1 to disable (e.g. self-signed test endpoints).
    const char* ca_bundle = get_ca_bundle_path();
    if (ca_bundle) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);
    }

    const char* insecure_env = std::getenv("UTP_INSECURE_SKIP_VERIFY");
    bool allow_insecure = (insecure_env && std::string(insecure_env) == "1");
    if (allow_insecure) {
        static std::once_flag warn_once;
        std::call_once(warn_once, []() {
            std::cerr << "[UTP Security Warning] UTP_INSECURE_SKIP_VERIFY=1 is active; TLS certificate verification is disabled.\n";
        });
    }
    long verify_peer = allow_insecure ? 0L : 1L;
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_peer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_peer ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

    // Re-use persistent TCP/TLS connections and DNS cache across all requests
    if (g_curl_share) {
        curl_easy_setopt(curl, CURLOPT_SHARE, g_curl_share);
    }
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

    // Network speed optimizations for large file transfers
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 2 * 1024 * 1024L); // 2MB receive buffer
    curl_easy_setopt(curl, CURLOPT_UPLOAD_BUFFERSIZE, 2 * 1024 * 1024L); // 2MB upload buffer
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);       // Keep connections alive
    
    // Prevent indefinite hanging if the TCP socket silently dies
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10L); // 10 bytes/sec limit
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);  // for 30 seconds

    // Prevent 1-second delay waiting for 100-continue response on chunk uploads
    chunk = curl_slist_append(chunk, "Expect:");

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    
    // Smooth real-time UI tracking for giant chunks
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, XferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &last_transfer_bytes);

    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::string err = curl_easy_strerror(res);
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        throw std::runtime_error("CURL request failed: " + err);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = static_cast<int>(http_code);

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    
    return response;
}

HttpResponse HttpClient::get(const std::string& url, const std::vector<std::string>& headers) {
    return perform_request(url, "GET", nullptr, 0, headers);
}

HttpResponse HttpClient::head(const std::string& url, const std::vector<std::string>& headers) {
    return perform_request(url, "HEAD", nullptr, 0, headers);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& data, const std::vector<std::string>& headers) {
    return perform_request(url, "POST", reinterpret_cast<const uint8_t*>(data.c_str()), data.size(), headers);
}

HttpResponse HttpClient::put(const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers) {
    return perform_request(url, "PUT", data, size, headers);
}

HttpResponse HttpClient::patch(const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers) {
    return perform_request(url, "PATCH", data, size, headers);
}

HttpResponse HttpClient::del(const std::string& url, const std::vector<std::string>& headers) {
    return perform_request(url, "DELETE", nullptr, 0, headers);
}

HttpResponse HttpClient::perform_custom_request(const std::string& method, const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers) {
    return perform_request(url, method.c_str(), data, size, headers);
}

uint64_t HttpClient::network_dl_bytes() { return s_network_dl_tick.load(); }
uint64_t HttpClient::network_ul_bytes() { return s_network_ul_tick.load(); }
void     HttpClient::reset_network_bytes() { s_network_dl_tick = 0; s_network_ul_tick = 0; }

} // namespace utp
