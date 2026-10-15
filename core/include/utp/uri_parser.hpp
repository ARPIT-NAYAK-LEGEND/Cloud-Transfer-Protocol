#pragma once

#include <string>
#include <stdexcept>

namespace utp {

struct Resource {
    std::string scheme;
    std::string account;
    std::string host_or_bucket;
    std::string path;
};

Resource parse_uri(const std::string& uri);

} // namespace utp
