#pragma once
#include "utp/types.hpp"
#include <vector>
#include <cstdint>

namespace utp {

struct SystemProfile {
    uint64_t total_ram_bytes{0};
    uint64_t available_ram_bytes{0};
    uint64_t ram_budget_bytes{0};
};

class ChunkPlanner {
public:
    // Queries the host OS for physical RAM and calculates safe memory budget
    static SystemProfile query_system_profile();

    // Dynamically computes optimal chunk size considering RAM, bandwidth, provider limits and target flight time
    static uint64_t calculate_chunk_size(
        uint64_t file_size,
        const ProviderCapabilities& source_caps,
        const ProviderCapabilities& dest_caps,
        uint64_t estimated_bandwidth_bps = 0
    );

    // Plans chunk distribution with hardware RAM and network bandwidth awareness
    static std::vector<ChunkInfo> plan(
        uint64_t file_size,
        const ProviderCapabilities& source_caps,
        const ProviderCapabilities& dest_caps,
        uint64_t estimated_bandwidth_bps = 0
    );
};

} // namespace utp
