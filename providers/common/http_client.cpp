#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <atomic>
#include <mutex>

namespace utp {

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

} // end namespace utp temporarily
extern std::atomic<uint64_t> g_bytes_transferred;
extern std::atomic<uint64_t> g_network_dl_tick;
extern std::atomic<uint64_t> g_network_ul_tick;
namespace utp { // reopen

static int XferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    uint64_t* last_bytes = static_cast<uint64_t*>(clientp);
    uint64_t last_dl = last_bytes[0];
    uint64_t last_ul = last_bytes[1];
    
    if (static_cast<uint64_t>(dlnow) > last_dl) {
        uint64_t diff = static_cast<uint64_t>(dlnow) - last_dl;
        g_network_dl_tick += diff;
        last_bytes[0] = dlnow;
    }
    
    if (static_cast<uint64_t>(ulnow) > last_ul) {
        uint64_t diff = static_cast<uint64_t>(ulnow) - last_ul;
        g_network_ul_tick += diff;
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

    // Disable SSL peer verification for testing to avoid MSYS2 CA-cert issues
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

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

} // namespace utp
