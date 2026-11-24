#include "utp/chunk_planner.hpp"
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace utp {

SystemProfile ChunkPlanner::query_system_profile() {
    SystemProfile profile{};

#ifdef _WIN32
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&mem_info)) {
        profile.total_ram_bytes = mem_info.ullTotalPhys;
        profile.available_ram_bytes = mem_info.ullAvailPhys;
    }
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #if defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
    long total_pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (total_pages > 0 && page_size > 0) {
        profile.total_ram_bytes = static_cast<uint64_t>(total_pages) * static_cast<uint64_t>(page_size);
    }
    #endif
    #if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGE_SIZE)
    long avail_pages = sysconf(_SC_AVPHYS_PAGES);
    if (avail_pages > 0 && page_size > 0) {
        profile.available_ram_bytes = static_cast<uint64_t>(avail_pages) * static_cast<uint64_t>(page_size);
    }
    #endif
#endif

    // Fallbacks if system memory query returned 0
    if (profile.available_ram_bytes == 0) {
        profile.available_ram_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 2 GB conservative default
    }
    if (profile.total_ram_bytes == 0) {
        profile.total_ram_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    }

    // Allocate at most 5% of available free physical RAM for chunk buffer
    // Clamp safely between 5 MB (floor for S3/R2 multipart compliance) and 64 MB (maximum single chunk buffer)
    uint64_t budget = profile.available_ram_bytes * 5 / 100;
    if (budget < 5ULL * 1024ULL * 1024ULL) budget = 5ULL * 1024ULL * 1024ULL;
    if (budget > 64ULL * 1024ULL * 1024ULL) budget = 64ULL * 1024ULL * 1024ULL;
    profile.ram_budget_bytes = budget;

    return profile;
}

uint64_t ChunkPlanner::calculate_chunk_size(
    uint64_t file_size,
    const ProviderCapabilities& source_caps,
    const ProviderCapabilities& dest_caps,
    uint64_t estimated_bandwidth_bps
) {
    // 1280 KB (1.25 MB = 1,310,720 bytes)
    // Divisible by Google Drive (256 KB * 5) and OneDrive (320 KB * 4).
    // AWS S3 / Cloudflare R2 / Backblaze B2 requires multipart parts >= 5 MB (which is 4 * align = 5.0 MB).
    const uint64_t align = 1280ULL * 1024ULL;

    // Small files under 1 alignment unit transfer as a single whole chunk
    if (file_size <= align) {
        return file_size;
    }

    // 1. Hardware RAM constraint profiling
    auto profile = query_system_profile();
    uint64_t max_ram_budget = profile.ram_budget_bytes;

    uint64_t k_min = 1;
    uint64_t k_max = std::max<uint64_t>(1, max_ram_budget / align);

    uint64_t k = 2; // Default starting multiplier: 2.5 MB (2 * 1.25 MB)

    // 2. Bandwidth-aware dynamic flight time sizing
    if (estimated_bandwidth_bps > 0) {
        // Target: ~2.5 seconds flight time per chunk for optimal pipeline throughput and UI responsiveness
        double target_bytes = static_cast<double>(estimated_bandwidth_bps) * 2.5;
        k = static_cast<uint64_t>(std::round(target_bytes / static_cast<double>(align)));
        if (k < 1) k = 1;
    } else {
        // Adaptive scaling tuned for responsive 2-3s UI cadence:
        // - < 30 MB: 1.25 MB (k = 1)
        // - 30 MB to 150 MB: 2.5 MB to 5.0 MB (k = 2 to 4) --> 112 MB file chunks are 5 MB, taking ~3-4s each!
        // - 150 MB to 500 MB: 5.0 MB (k = 4)
        // - 500 MB to 2 GB: 10.0 MB (k = 8)
        // - > 2 GB: 20.0 MB (k = 16)
        if (file_size < 30ULL * 1024ULL * 1024ULL) {
            k = 1; // 1.25 MB
        } else if (file_size < 150ULL * 1024ULL * 1024ULL) {
            k = (file_size <= 60ULL * 1024ULL * 1024ULL) ? 2 : 4; // 2.5 MB or 5.0 MB
        } else if (file_size < 500ULL * 1024ULL * 1024ULL) {
            k = 4; // 5.0 MB
        } else if (file_size < 2048ULL * 1024ULL * 1024ULL) {
            k = 8; // 10.0 MB
        } else {
            k = 16; // 20.0 MB
        }
    }

    // Clamp k within hardware RAM budget
    k = std::clamp(k, k_min, k_max);

    uint64_t chunk_size = k * align;

    // Ensure that if file is large enough to be chunked into multiple parts,
    // a single chunk never consumes more than half the file (guaranteeing at least 2-4 chunks)
    if (file_size >= 4 * align && chunk_size > file_size / 2) {
        chunk_size = std::max(align, (file_size / 4 / align) * align);
    }

    // 3. Provider capability limits enforcement
    if (source_caps.max_chunk_size > 0 && chunk_size > source_caps.max_chunk_size) {
        chunk_size = source_caps.max_chunk_size;
    }
    if (dest_caps.max_chunk_size > 0 && chunk_size > dest_caps.max_chunk_size) {
        chunk_size = dest_caps.max_chunk_size;
    }

    if (chunk_size == 0) chunk_size = align;
    return chunk_size;
}

std::vector<ChunkInfo> ChunkPlanner::plan(
    uint64_t file_size,
    const ProviderCapabilities& source_caps,
    const ProviderCapabilities& dest_caps,
    uint64_t estimated_bandwidth_bps
) {
    std::vector<ChunkInfo> chunks;
    if (file_size == 0) {
        return chunks;
    }

    uint64_t chunk_size = calculate_chunk_size(file_size, source_caps, dest_caps, estimated_bandwidth_bps);

    if (file_size <= chunk_size) {
        chunk_size = file_size;
    } else {
        // P1.6: honor capability matrix — a provider without ranged reads
        // cannot serve byte ranges. Fail fast with clear error instead.
        if (!source_caps.ranged_read) {
            throw TransferError(ErrorCategory::FATAL,
                "Source provider does not support ranged reads; cannot chunk a " +
                std::to_string(file_size) + "-byte file. Transfer whole files only.");
        }
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
