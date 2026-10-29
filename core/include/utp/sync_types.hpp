#pragma once

#include "utp/types.hpp"
#include <string>
#include <vector>
#include <functional>

namespace utp {

enum class SyncDirection {
    UNIDIRECTIONAL, // Source -> Destination
    BIDIRECTIONAL   // Source <-> Destination (two-way merge)
};

enum class ConflictPolicy {
    FAIL,        // Abort transfer on conflict
    OVERWRITE,   // Overwrite conflicting destination file
    RENAME,      // Keep destination, save new file as name.conflict_<ts>.ext
    SKIP,        // Skip conflicting file, leaving both unchanged
    NEWER_WINS   // Compare modification timestamps; newer timestamp wins
};

inline const char* conflict_policy_to_string(ConflictPolicy p) {
    switch (p) {
        case ConflictPolicy::FAIL: return "FAIL";
        case ConflictPolicy::OVERWRITE: return "OVERWRITE";
        case ConflictPolicy::RENAME: return "RENAME";
        case ConflictPolicy::SKIP: return "SKIP";
        case ConflictPolicy::NEWER_WINS: return "NEWER_WINS";
    }
    return "OVERWRITE";
}

inline ConflictPolicy conflict_policy_from_string(const std::string& str) {
    if (str == "FAIL" || str == "fail") return ConflictPolicy::FAIL;
    if (str == "RENAME" || str == "rename") return ConflictPolicy::RENAME;
    if (str == "SKIP" || str == "skip") return ConflictPolicy::SKIP;
    if (str == "NEWER_WINS" || str == "newer_wins" || str == "NEWER" || str == "newer") return ConflictPolicy::NEWER_WINS;
    return ConflictPolicy::OVERWRITE;
}

enum class SyncActionType {
    CREATE,   // File exists only on source (or dest in bidirectional); transfer needed
    UPDATE,   // File modified on source (size or mtime changed); transfer needed
    SKIP,     // File identical on source & dest (same size and mtime); zero bytes transferred
    CONFLICT  // Conflict detected (different contents/mtimes requiring policy resolution)
};

inline const char* sync_action_to_string(SyncActionType a) {
    switch (a) {
        case SyncActionType::CREATE: return "CREATE";
        case SyncActionType::UPDATE: return "UPDATE";
        case SyncActionType::SKIP: return "SKIP";
        case SyncActionType::CONFLICT: return "CONFLICT";
    }
    return "UNKNOWN";
}

struct SyncItem {
    std::string rel_path;          // Relative path (e.g. "docs/report.pdf")
    std::string src_path;          // Source path
    std::string dest_path;         // Destination path
    std::string target_path;       // Actual target write path (may differ if renamed due to conflict)
    uint64_t src_size{0};
    uint64_t dest_size{0};
    uint64_t src_mtime{0};
    uint64_t dest_mtime{0};
    std::string src_hash;
    std::string dest_hash;
    SyncActionType action{SyncActionType::CREATE};
    ConflictPolicy resolved_policy{ConflictPolicy::OVERWRITE};
    bool copy_dest_to_src{false};  // In bidirectional sync, true if dest wins and propagates to src
    std::string status{"PENDING"}; // PENDING, SKIPPED, COMPLETED, FAILED, CONFLICT_RESOLVED
    std::string error_message;
};

struct SyncPlan {
    std::string id;
    std::string src_scheme;
    std::string src_root;
    std::string dest_scheme;
    std::string dest_root;
    SyncDirection direction{SyncDirection::UNIDIRECTIONAL};
    ConflictPolicy policy{ConflictPolicy::OVERWRITE};
    std::vector<SyncItem> items;
    uint64_t total_bytes_to_transfer{0};
    uint64_t files_to_create{0};
    uint64_t files_to_update{0};
    uint64_t files_to_skip{0};
    uint64_t conflicts_detected{0};
};

struct SyncResult {
    std::string sync_id;
    bool success{false};
    uint64_t files_transferred{0};
    uint64_t files_skipped{0};
    uint64_t conflicts_resolved{0};
    uint64_t bytes_transferred{0};
    std::vector<std::string> errors;
};

using SyncProgressCallback = std::function<void(const SyncItem& item, size_t item_index, size_t total_items, uint64_t item_bytes_done, uint64_t item_total_bytes)>;

} // namespace utp
