#include "utp/uri_parser.hpp"

namespace utp {

Resource parse_uri(const std::string& uri) {
    Resource res;

    // Find scheme
    size_t scheme_end = uri.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0) {
        throw std::invalid_argument("Invalid URI: missing or malformed scheme");
    }
    res.scheme = uri.substr(0, scheme_end);

    // Find host and path
    size_t rest_start = scheme_end + 3; // length of "://"
    size_t path_start = uri.find("/", rest_start);

    if (path_start == std::string::npos) {
        res.host_or_bucket = uri.substr(rest_start);
        res.path = "";
    } else {
        res.host_or_bucket = uri.substr(rest_start, path_start - rest_start);
        res.path = uri.substr(path_start + 1); // skip the '/' itself for the internal path logic
    }

    // Edge cases
    if (res.scheme.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.") != std::string::npos) {
        throw std::invalid_argument("Invalid URI: invalid characters in scheme");
    }
    
    // For local, we often just want the path, but let's conform to typical structure or keep host empty
    // Actually, local:///path means host is empty, and path is /path.
    // wait, if we have local:///path, rest_start is "://"
    // "local:///path" -> scheme="local", rest="/path". 
    // uri.find("/") on "/path" is at index 0 of rest. So host is empty, path is "path".
    // That means local:///path translates to host="", path="path".
    
    // If it's local://path, host="path", path=""
    // But usually it's local:///path

    return res;
}

} // namespace utp
