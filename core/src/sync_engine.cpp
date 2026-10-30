#include "utp/sync_engine.hpp"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>

namespace utp {

SyncEngine::SyncEngine(std::shared_ptr<StorageProvider> source,
                       std::shared_ptr<StorageProvider> dest,
                       const std::string& src_scheme,
                       const std::string& dest_scheme,
                       const std::string& db_path)
    : source_(std::move(source)),
      dest_(std::move(dest)),
      src_scheme_(src_scheme),
      dest_scheme_(dest_scheme),
      db_path_(db_path),
      state_manager_(db_path) {}

std::string SyncEngine::generate_sync_id() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    static std::mt19937_64 rng(static_cast<uint64_t>(now));
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t r = dist(rng);
    std::stringstream ss;
    ss << "sync_" << std::hex << (now & 0xFFFFFFFF) << (r & 0xFFFF);
    return ss.str();
}

std::string SyncEngine::normalize_path_separators(const std::string& p) {
    std::string s;
    s.reserve(p.size());
    for (char c : p) {
        if (c == '\\') s.push_back('/');
        else s.push_back(c);
    }
    // Remove duplicate slashes
    std::string res;
    res.reserve(s.size());
    bool last_slash = false;
    for (char c : s) {
        if (c == '/') {
            if (!last_slash) res.push_back(c);
            last_slash = true;
        } else {
            res.push_back(c);
            last_slash = false;
        }
    }
    return res;
}

std::string SyncEngine::combine_path(const std::string& base, const std::string& rel) {
    if (base.empty()) return rel;
    if (rel.empty()) return base;
    std::string b = normalize_path_separators(base);
    std::string r = normalize_path_separators(rel);
    while (!b.empty() && b.back() == '/') b.pop_back();
    size_t r_start = 0;
    while (r_start < r.size() && r[r_start] == '/') r_start++;
    return b + "/" + r.substr(r_start);
}

void SyncEngine::traverse_dir(std::shared_ptr<StorageProvider> provider,
                             const std::string& current_path,
                             const std::string& base_root,
                             std::vector<FileInfo>& collected,
                             std::set<std::string>& visited) {
    std::string norm_curr = normalize_path_separators(current_path);
    if (!norm_curr.empty() && visited.count(norm_curr)) return;
    visited.insert(norm_curr);

    std::vector<FileInfo> entries;
    try {
        entries = provider->list(current_path);
    } catch (...) {
        return;
    }

    std::string norm_base = normalize_path_separators(base_root);
    while (!norm_base.empty() && norm_base.back() == '/') norm_base.pop_back();

    for (const auto& entry : entries) {
        std::string norm_path = normalize_path_separators(entry.path);
        if (entry.is_directory) {
            // Check for loop
            if (visited.find(norm_path) == visited.end()) {
                traverse_dir(provider, entry.path, base_root, collected, visited);
            }
        } else {
            // Compute relative path from base_root
            std::string rel;
            if (!norm_base.empty() && norm_path.rfind(norm_base, 0) == 0) {
                rel = norm_path.substr(norm_base.size());
            } else {
                rel = norm_path;
            }
            while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
                rel.erase(rel.begin());
            }
            if (rel.empty()) rel = entry.name.empty() ? norm_path : entry.name;

            FileInfo item = entry;
            item.name = rel; // Store relative path in name
            collected.push_back(item);
        }
    }
}

std::vector<FileInfo> SyncEngine::list_recursive(std::shared_ptr<StorageProvider> provider, const std::string& root_path) {
    std::vector<FileInfo> collected;
    std::set<std::string> visited;
    traverse_dir(provider, root_path, root_path, collected, visited);
    return collected;
}

SyncPlan SyncEngine::plan_sync(const std::string& src_root,
                              const std::string& dest_root,
                              SyncDirection direction,
                              ConflictPolicy policy) {
    SyncPlan plan;
    plan.id = generate_sync_id();
    plan.src_scheme = src_scheme_;
    plan.src_root = src_root;
    plan.dest_scheme = dest_scheme_;
    plan.dest_root = dest_root;
    plan.direction = direction;
    plan.policy = policy;

    auto src_files = list_recursive(source_, src_root);
    auto dest_files = list_recursive(dest_, dest_root);

    std::map<std::string, FileInfo> src_map;
    for (const auto& f : src_files) {
        src_map[f.name] = f;
    }

    std::map<std::string, FileInfo> dest_map;
    for (const auto& f : dest_files) {
        dest_map[f.name] = f;
    }

    std::set<std::string> all_keys;
    for (const auto& kv : src_map) all_keys.insert(kv.first);
    for (const auto& kv : dest_map) all_keys.insert(kv.first);

    for (const auto& rel : all_keys) {
        bool in_src = src_map.count(rel) > 0;
        bool in_dest = dest_map.count(rel) > 0;

        if (in_src && !in_dest) {
            const auto& sf = src_map[rel];
            SyncItem item;
            item.rel_path = rel;
            item.src_path = sf.path;
            item.dest_path = combine_path(dest_root, rel);
            item.target_path = item.dest_path;
            item.src_size = sf.size;
            item.dest_size = 0;
            item.src_mtime = sf.mtime;
            item.dest_mtime = 0;
            item.action = SyncActionType::CREATE;
            item.resolved_policy = policy;
            item.copy_dest_to_src = false;
            plan.files_to_create++;
            plan.total_bytes_to_transfer += sf.size;
            plan.items.push_back(item);
        } else if (!in_src && in_dest) {
            const auto& df = dest_map[rel];
            if (direction == SyncDirection::BIDIRECTIONAL) {
                // In bidirectional mode, new file in destination is copied to source
                SyncItem item;
                item.rel_path = rel;
                item.src_path = combine_path(src_root, rel);
                item.dest_path = df.path;
                item.target_path = item.src_path;
                item.src_size = 0;
                item.dest_size = df.size;
                item.src_mtime = 0;
                item.dest_mtime = df.mtime;
                item.action = SyncActionType::CREATE;
                item.resolved_policy = policy;
                item.copy_dest_to_src = true; // Destination to Source
                plan.files_to_create++;
                plan.total_bytes_to_transfer += df.size;
                plan.items.push_back(item);
            } else {
                // In unidirectional mode, leave destination files as-is
                SyncItem item;
                item.rel_path = rel;
                item.src_path = "";
                item.dest_path = df.path;
                item.target_path = df.path;
                item.src_size = 0;
                item.dest_size = df.size;
                item.action = SyncActionType::SKIP;
                item.status = "SKIPPED";
                plan.files_to_skip++;
                plan.items.push_back(item);
            }
        } else {
            // File exists on both sides
            const auto& sf = src_map[rel];
            const auto& df = dest_map[rel];

            bool size_equal = (sf.size == df.size);
            bool mtime_equal = (sf.mtime != 0 && df.mtime != 0 &&
                                std::abs(static_cast<int64_t>(sf.mtime) - static_cast<int64_t>(df.mtime)) <= 1);

            if (size_equal && (mtime_equal || (sf.mtime == 0 && df.mtime == 0))) {
                // Identical file: SKIP (zero byte transfer!)
                SyncItem item;
                item.rel_path = rel;
                item.src_path = sf.path;
                item.dest_path = df.path;
                item.target_path = df.path;
                item.src_size = sf.size;
                item.dest_size = df.size;
                item.src_mtime = sf.mtime;
                item.dest_mtime = df.mtime;
                item.action = SyncActionType::SKIP;
                item.status = "SKIPPED";
                plan.files_to_skip++;
                plan.items.push_back(item);
            } else {
                // Difference detected! Conflict resolution policy applies
                plan.conflicts_detected++;
                SyncItem item;
                item.rel_path = rel;
                item.src_path = sf.path;
                item.dest_path = df.path;
                item.src_size = sf.size;
                item.dest_size = df.size;
                item.src_mtime = sf.mtime;
                item.dest_mtime = df.mtime;
                item.resolved_policy = policy;

                switch (policy) {
                    case ConflictPolicy::FAIL:
                        item.action = SyncActionType::CONFLICT;
                        item.error_message = "Conflict detected between source and destination: " + rel;
                        plan.items.push_back(item);
                        break;
                    case ConflictPolicy::SKIP:
                        item.action = SyncActionType::SKIP;
                        item.status = "SKIPPED";
                        plan.files_to_skip++;
                        plan.items.push_back(item);
                        break;
                    case ConflictPolicy::OVERWRITE:
                        item.action = SyncActionType::UPDATE;
                        item.target_path = df.path;
                        item.copy_dest_to_src = false;
                        plan.files_to_update++;
                        plan.total_bytes_to_transfer += sf.size;
                        plan.items.push_back(item);
                        break;
                    case ConflictPolicy::RENAME: {
                        item.action = SyncActionType::UPDATE;
                        // Format conflict rename: name.conflict_TIMESTAMP.ext
                        auto dot_pos = rel.rfind('.');
                        std::string base_name = (dot_pos == std::string::npos) ? rel : rel.substr(0, dot_pos);
                        std::string ext = (dot_pos == std::string::npos) ? "" : rel.substr(dot_pos);
                        auto ts = std::chrono::system_clock::now().time_since_epoch().count();
                        std::string renamed_rel = base_name + ".conflict_" + std::to_string(ts & 0xFFFFFF) + ext;
                        item.target_path = combine_path(dest_root, renamed_rel);
                        item.copy_dest_to_src = false;
                        plan.files_to_create++;
                        plan.total_bytes_to_transfer += sf.size;
                        plan.items.push_back(item);
                        break;
                    }
                    case ConflictPolicy::NEWER_WINS: {
                        if (sf.mtime >= df.mtime) {
                            // Source is newer or equal
                            item.action = SyncActionType::UPDATE;
                            item.target_path = df.path;
                            item.copy_dest_to_src = false;
                            plan.files_to_update++;
                            plan.total_bytes_to_transfer += sf.size;
                            plan.items.push_back(item);
                        } else {
                            // Destination is newer
                            if (direction == SyncDirection::BIDIRECTIONAL) {
                                item.action = SyncActionType::UPDATE;
                                item.target_path = sf.path;
                                item.copy_dest_to_src = true; // Destination to Source
                                plan.files_to_update++;
                                plan.total_bytes_to_transfer += df.size;
                                plan.items.push_back(item);
                            } else {
                                item.action = SyncActionType::SKIP;
                                item.status = "SKIPPED";
                                plan.files_to_skip++;
                                plan.items.push_back(item);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    return plan;
}

void SyncEngine::emit_event(const std::string& sync_id, EventType type, const std::string& payload, EventCallback cb) {
    try {
        state_manager_.log_event(sync_id, type, payload);
    } catch (...) {}
    if (cb) {
        Event ev;
        ev.transfer_id = sync_id;
        ev.type = type;
        ev.payload = payload;
        ev.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        try { cb(ev); } catch (...) {}
    }
}

SyncResult SyncEngine::execute_sync(const SyncPlan& plan,
                                   SyncProgressCallback progress_cb,
                                   EventCallback event_cb) {
    SyncResult result;
    result.sync_id = plan.id;
    result.success = false;

    // Check for unresolvable conflicts under FAIL policy
    if (plan.policy == ConflictPolicy::FAIL && plan.conflicts_detected > 0) {
        result.errors.push_back("Sync aborted: " + std::to_string(plan.conflicts_detected) + " conflicts detected under FAIL policy.");
        emit_event(plan.id, EventType::SYNC_FAILED, "Aborted: " + std::to_string(plan.conflicts_detected) + " conflicts", event_cb);
        return result;
    }

    // Persist plan in state manager
    try {
        state_manager_.save_sync_plan(plan);
    } catch (...) {}

    emit_event(plan.id, EventType::SYNC_STARTED, "Total items: " + std::to_string(plan.items.size()), event_cb);
    emit_event(plan.id, EventType::SYNC_PLAN_READY,
               "To transfer: " + std::to_string(plan.files_to_create + plan.files_to_update) +
               ", To skip: " + std::to_string(plan.files_to_skip), event_cb);

    size_t total_items = plan.items.size();
    for (size_t i = 0; i < total_items; ++i) {
        const auto& item = plan.items[i];

        if (item.action == SyncActionType::SKIP) {
            result.files_skipped++;
            if (progress_cb) {
                progress_cb(item, i, total_items, item.src_size, item.src_size);
            }
            continue;
        }

        try {
            if (item.copy_dest_to_src) {
                // Reverse direction: Destination -> Source
                Engine engine(dest_, source_, db_path_);
                std::string tid = engine.transfer(dest_scheme_, item.dest_path, src_scheme_, item.target_path,
                    [&](uint64_t bytes, uint64_t total) {
                        if (progress_cb) progress_cb(item, i, total_items, bytes, total);
                    });
                result.files_transferred++;
                result.bytes_transferred += item.dest_size;
                if ((src_scheme_ == "file" || src_scheme_ == "local") && item.dest_mtime > 0) {
                    std::error_code ec;
                    auto sctp = std::chrono::system_clock::from_time_t(item.dest_mtime);
                    std::filesystem::last_write_time(item.target_path, std::chrono::file_clock::from_sys(sctp), ec);
                }
            } else {
                // Standard direction: Source -> Destination
                Engine engine(source_, dest_, db_path_);
                std::string tid = engine.transfer(src_scheme_, item.src_path, dest_scheme_, item.target_path,
                    [&](uint64_t bytes, uint64_t total) {
                        if (progress_cb) progress_cb(item, i, total_items, bytes, total);
                    });
                result.files_transferred++;
                result.bytes_transferred += item.src_size;
                if ((dest_scheme_ == "file" || dest_scheme_ == "local") && item.src_mtime > 0) {
                    std::error_code ec;
                    auto sctp = std::chrono::system_clock::from_time_t(item.src_mtime);
                    std::filesystem::last_write_time(item.target_path, std::chrono::file_clock::from_sys(sctp), ec);
                }
            }

            if (item.action == SyncActionType::UPDATE) {
                result.conflicts_resolved++;
                emit_event(plan.id, EventType::SYNC_CONFLICT_RESOLVED,
                           item.rel_path + " (" + conflict_policy_to_string(item.resolved_policy) + ")", event_cb);
            }

            try {
                state_manager_.update_sync_item_status(plan.id, item.rel_path, "COMPLETED");
            } catch (...) {}

            emit_event(plan.id, EventType::SYNC_ITEM_COMPLETED, item.rel_path, event_cb);
        } catch (const std::exception& e) {
            result.errors.push_back("Failed to sync " + item.rel_path + ": " + e.what());
            try {
                state_manager_.update_sync_item_status(plan.id, item.rel_path, "FAILED", e.what());
            } catch (...) {}
        }
    }

    result.success = result.errors.empty();
    try {
        state_manager_.complete_sync(plan.id, result.files_transferred, result.files_skipped,
                                     result.conflicts_resolved, result.bytes_transferred);
    } catch (...) {}

    emit_event(plan.id, result.success ? EventType::SYNC_COMPLETED : EventType::SYNC_FAILED,
               "Transferred: " + std::to_string(result.files_transferred) +
               ", Skipped: " + std::to_string(result.files_skipped) +
               ", Errors: " + std::to_string(result.errors.size()), event_cb);

    return result;
}

} // namespace utp
