#include "utp/uri_parser.hpp"

namespace utp {

Resource parse_uri(const std::string& uri) {
    Resource res;

    // Find scheme delimiter
    size_t scheme_end = uri.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0) {
        throw std::invalid_argument("Invalid URI: missing or malformed scheme");
    }
    std::string raw_scheme = uri.substr(0, scheme_end);

    // Check if account is in scheme: e.g. "gdrive:my_work" or "onedrive+personal"
    size_t colon_pos = raw_scheme.find(':');
    size_t plus_pos = raw_scheme.find('+');
    size_t delim_pos = (colon_pos != std::string::npos) ? colon_pos : plus_pos;
    if (delim_pos != std::string::npos && delim_pos > 0) {
        res.scheme = raw_scheme.substr(0, delim_pos);
        res.account = raw_scheme.substr(delim_pos + 1);
    } else {
        res.scheme = raw_scheme;
    }

    // Find rest after "://"
    std::string rest = uri.substr(scheme_end + 3);

    // Check for userinfo / account: e.g. "my_work@..."
    size_t slash_pos = rest.find('/');
    size_t at_pos = rest.find('@');
    if (at_pos != std::string::npos && (slash_pos == std::string::npos || at_pos < slash_pos)) {
        if (res.account.empty()) {
            res.account = rest.substr(0, at_pos);
        }
        rest = rest.substr(at_pos + 1);
    }

    // Find host and path
    size_t path_start = rest.find('/');
    if (path_start == std::string::npos) {
        res.host_or_bucket = rest;
        res.path = "";
    } else {
        res.host_or_bucket = rest.substr(0, path_start);
        res.path = rest.substr(path_start + 1);
    }

    // Validate scheme
    if (res.scheme.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.") != std::string::npos) {
        throw std::invalid_argument("Invalid URI: invalid characters in scheme");
    }

    return res;
}

} // namespace utp
