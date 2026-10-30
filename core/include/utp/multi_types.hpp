#pragma once

#include "providers/interface/storage_provider.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace utp {

enum class MultiTransferPolicy {
    ALL_OR_NOTHING,  // If any destination fails, all destinations are aborted
    BEST_EFFORT      // If a destination fails, other destinations continue to completion
};

inline const char* multi_transfer_policy_to_string(MultiTransferPolicy p) {
    switch (p) {
        case MultiTransferPolicy::ALL_OR_NOTHING: return "ALL_OR_NOTHING";
        case MultiTransferPolicy::BEST_EFFORT:    return "BEST_EFFORT";
    }
    return "ALL_OR_NOTHING";
}

inline MultiTransferPolicy multi_transfer_policy_from_string(const std::string& s) {
    if (s == "BEST_EFFORT" || s == "best_effort" || s == "best-effort") {
        return MultiTransferPolicy::BEST_EFFORT;
    }
    return MultiTransferPolicy::ALL_OR_NOTHING;
}

struct DestinationTarget {
    std::string scheme;
    Path path;
    std::shared_ptr<StorageProvider> provider;
    std::string display_name = "";
};

struct DestinationProgress {
    size_t target_index{0};
    std::string display_name;
    std::string scheme;
    Path path;
    uint64_t bytes_transferred{0};
    uint64_t total_bytes{0};
    bool active{true};
    bool completed{false};
    bool failed{false};
    std::string error = "";
};

using MultiProgressCallback = std::function<void(const std::vector<DestinationProgress>& progress_list)>;

struct DestinationResult {
    size_t target_index{0};
    std::string scheme;
    Path path;
    std::string display_name;
    bool success{false};
    uint64_t bytes_transferred{0};
    std::string sha256 = "";
    std::string error = "";
    double duration_seconds{0.0};
};

struct MultiTransferResult {
    std::string multi_id;
    bool overall_success{false};
    uint64_t total_source_size{0};
    std::vector<DestinationResult> destinations;
    std::vector<std::string> errors;
    double total_duration_seconds{0.0};
};

} // namespace utp
