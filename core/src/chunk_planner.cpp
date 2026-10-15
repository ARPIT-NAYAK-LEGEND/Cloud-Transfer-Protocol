#include "utp/chunk_planner.hpp"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace utp {

std::vector<ChunkInfo> ChunkPlanner::plan(uint64_t file_size, const ProviderCapabilities& source_caps, const ProviderCapabilities& dest_caps) {
    std::vector<ChunkInfo> chunks;
    
    // 1280 KB (1.25 MB = 1,310,720 bytes)
    // Divisible by Google Drive (256KB * 5) and OneDrive (320KB * 4) and S3 (>= 5MB)
    const uint64_t align = 1280ULL * 1024ULL;
    
    // High-performance adaptive chunk size:
    // Scale chunk size with file size to minimize HTTP request round-trips:
    // - Under 20 MB: 2.5 MB chunks (2 * align)
    // - 20 MB to 100 MB: 10.24 MB chunks (8 * align)
    // - 100 MB to 500 MB: 20.48 MB chunks (16 * align)
    // - Above 500 MB: 40.96 MB chunks (32 * align) — turns 200+ HTTP round-trips into ~25!
    uint64_t chunk_size = 4 * align; // 5 MB default
    if (file_size < 20ULL * 1024ULL * 1024ULL) {
        chunk_size = 2 * align;
    } else if (file_size < 100ULL * 1024ULL * 1024ULL) {
        chunk_size = 8 * align;
    } else if (file_size < 500ULL * 1024ULL * 1024ULL) {
        chunk_size = 16 * align;
    } else {
        chunk_size = 32 * align;
    }

    if (file_size <= chunk_size) {
        chunk_size = file_size;
    } else {
        if (source_caps.max_chunk_size > 0 && chunk_size > source_caps.max_chunk_size) {
            chunk_size = source_caps.max_chunk_size;
        }
        if (dest_caps.max_chunk_size > 0 && chunk_size > dest_caps.max_chunk_size) {
            chunk_size = dest_caps.max_chunk_size;
        }
    }
    
    if (chunk_size == 0) chunk_size = align;
    
    uint64_t offset = 0;
    while (offset < file_size) {
        uint64_t size = std::min(chunk_size, file_size - offset);
        chunks.push_back({offset, size, ChunkState::PENDING});
        offset += size;
    }
    
    return chunks;
}

} // namespace utp
