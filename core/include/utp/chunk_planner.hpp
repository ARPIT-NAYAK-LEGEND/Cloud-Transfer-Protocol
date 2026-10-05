#pragma once
#include "utp/types.hpp"
#include <vector>

namespace utp {

class ChunkPlanner {
public:
    static std::vector<ChunkInfo> plan(uint64_t file_size, const ProviderCapabilities& source_caps, const ProviderCapabilities& dest_caps);
};

} // namespace utp
