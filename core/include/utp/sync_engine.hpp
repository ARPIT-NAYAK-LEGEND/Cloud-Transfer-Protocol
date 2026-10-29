#pragma once

#include "utp/sync_types.hpp"
#include "utp/engine.hpp"
#include "utp/state_manager.hpp"
#include "providers/interface/storage_provider.hpp"
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace utp {

class SyncEngine {
public:
    SyncEngine(std::shared_ptr<StorageProvider> source,
               std::shared_ptr<StorageProvider> dest,
               const std::string& src_scheme = "local",
               const std::string& dest_scheme = "local",
               const std::string& db_path = "utp.db");

    SyncPlan plan_sync(const std::string& src_root,
                       const std::string& dest_root,
                       SyncDirection direction = SyncDirection::UNIDIRECTIONAL,
                       ConflictPolicy policy = ConflictPolicy::OVERWRITE);

    SyncResult execute_sync(const SyncPlan& plan,
                            SyncProgressCallback progress_cb = nullptr,
                            EventCallback event_cb = nullptr);

    static std::vector<FileInfo> list_recursive(std::shared_ptr<StorageProvider> provider, const std::string& root_path);
    static std::string normalize_path_separators(const std::string& p);
    static std::string combine_path(const std::string& base, const std::string& rel);
    static std::string generate_sync_id();

private:
    std::shared_ptr<StorageProvider> source_;
    std::shared_ptr<StorageProvider> dest_;
    std::string src_scheme_;
    std::string dest_scheme_;
    std::string db_path_;
    StateManager state_manager_;

    static void traverse_dir(std::shared_ptr<StorageProvider> provider,
                             const std::string& current_path,
                             const std::string& base_root,
                             std::vector<FileInfo>& collected,
                             std::set<std::string>& visited);

    void emit_event(const std::string& sync_id, EventType type, const std::string& payload, EventCallback cb = nullptr);
};

} // namespace utp
