#include "utp/chunk_planner.hpp"
#include <algorithm>

namespace utp {

std::vector<ChunkInfo> ChunkPlanner::plan(uint64_t file_size, const ProviderCapabilities& source_caps, const ProviderCapabilities& dest_caps) {
    std::vector<ChunkInfo> chunks;
    
    // Default chunk size is 8MB
    uint64_t chunk_size = 8 * 1024 * 1024; 
    
    // Adjust based on provider capabilities if needed
    if (source_caps.max_chunk_size > 0) {
        chunk_size = std::min(chunk_size, source_caps.max_chunk_size);
    }
    if (dest_caps.max_chunk_size > 0) {
        chunk_size = std::min(chunk_size, dest_caps.max_chunk_size);
    }
    
    uint64_t offset = 0;
    while (offset < file_size) {
        uint64_t size = std::min(chunk_size, file_size - offset);
        chunks.push_back({offset, size, ChunkState::PENDING});
        offset += size;
    }
    
    return chunks;
}

} // namespace utp
