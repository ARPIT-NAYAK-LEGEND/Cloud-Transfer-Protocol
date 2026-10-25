#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <stdexcept>

namespace utp {

struct HttpResponse {
    int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(const std::string& url, const std::vector<std::string>& headers);
    HttpResponse head(const std::string& url, const std::vector<std::string>& headers);
    HttpResponse post(const std::string& url, const std::string& data, const std::vector<std::string>& headers);
    HttpResponse put(const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers);
    HttpResponse patch(const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers);
    HttpResponse del(const std::string& url, const std::vector<std::string>& headers);
    HttpResponse perform_custom_request(const std::string& method, const std::string& url, const uint8_t* data, size_t size, const std::vector<std::string>& headers);

    // Per-process network byte counters (updated by CURL transfer callbacks).
    // Used by the CLI HUD for live speed calculation.
    static uint64_t network_dl_bytes();
    static uint64_t network_ul_bytes();
    static void     reset_network_bytes();

private:
    HttpResponse perform_request(const std::string& url, const char* method, const uint8_t* data, size_t size, const std::vector<std::string>& headers);
};

} // namespace utp
