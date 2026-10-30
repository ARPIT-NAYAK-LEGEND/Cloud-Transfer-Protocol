#pragma once

#include "utp/types.hpp"
#include "utp/events.hpp"
#include "utp/sync_types.hpp"
#include "utp/multi_types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>

struct sqlite3;

namespace utp {

class StateManager {
public:
    explicit StateManager(const std::string& db_path);
    ~StateManager();

    // Disable copy/move for simplicity
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
    StateManager(StateManager&&) = delete;
    StateManager& operator=(StateManager&&) = delete;

    void init_schema();

    void create_transfer(const Transfer& transfer);
    void update_transfer_state(const std::string& transfer_id, TransferState state);
    void update_transfer_progress(const std::string& transfer_id, uint64_t bytes_completed);
    void update_transfer_size(const std::string& transfer_id, uint64_t bytes_total);
    void record_transfer_error(const std::string& transfer_id, const std::string& error, int http_code);

    std::optional<Transfer> get_transfer(const std::string& transfer_id);
    std::vector<Transfer> list_transfers(int limit = 50);
    std::vector<ChunkInfo> get_plan(const std::string& transfer_id);

    void save_plan(const std::string& transfer_id, const std::vector<ChunkInfo>& plan);
    void update_chunk_state(const std::string& transfer_id, uint64_t chunk_index, ChunkState state);
    void record_chunk_attempt(const std::string& transfer_id, uint64_t chunk_index);

    // P1.3: durable upload-session tokens, so a crash + resume continues the
    // same provider upload session instead of restarting from zero.
    void save_session_token(const std::string& transfer_id, const std::string& role, const std::string& token);
    std::string get_session_token(const std::string& transfer_id, const std::string& role);
    void delete_session_tokens(const std::string& transfer_id);

    // P2.1: event stream — one append-only ledger row per lifecycle event.
    void log_event(const std::string& transfer_id, EventType type, const std::string& payload = "");
    std::vector<Event> get_events(const std::string& transfer_id, int limit = 50);

    // P2.3: cross-process control plane — `utp pause/cancel <id>` writes a
    // control flag here; a running engine polls it between chunks.
    void request_control(const std::string& transfer_id, const std::string& action);
    std::string get_control(const std::string& transfer_id);
    void clear_control(const std::string& transfer_id);

    // Phase 6: Sync session & item persistence
    void save_sync_plan(const SyncPlan& plan);
    void update_sync_item_status(const std::string& sync_id, const std::string& rel_path, const std::string& status, const std::string& error = "");
    void complete_sync(const std::string& sync_id, uint64_t transferred_count, uint64_t skipped_count, uint64_t conflicts_count, uint64_t bytes_transferred);

    // Phase 7: Multi-destination fan-out persistence
    void create_multi_transfer(const std::string& multi_id, const std::string& source_uri, MultiTransferPolicy policy, const std::vector<DestinationTarget>& targets, uint64_t total_bytes);
    void update_multi_dest_progress(const std::string& multi_id, size_t dest_index, uint64_t bytes_transferred, const std::string& status, const std::string& error = "", const std::string& sha256 = "");
    void complete_multi_transfer(const std::string& multi_id, bool overall_success);

private:
    sqlite3* db_{nullptr};

    void execute(const std::string& sql);
    void add_column_if_missing(const std::string& table, const std::string& column, const std::string& ddl);
};

} // namespace utp
