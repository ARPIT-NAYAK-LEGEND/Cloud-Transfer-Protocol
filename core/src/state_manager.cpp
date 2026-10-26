#include <mutex>
#include "utp/state_manager.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace utp {

static std::recursive_mutex g_db_mutex;


static std::string state_to_string(TransferState state) {
    switch(state) {
        case TransferState::CREATED: return "CREATED";
        case TransferState::AUTHENTICATING: return "AUTHENTICATING";
        case TransferState::DISCOVERING: return "DISCOVERING";
        case TransferState::NEGOTIATING: return "NEGOTIATING";
        case TransferState::PLANNING: return "PLANNING";
        case TransferState::TRANSFERRING: return "TRANSFERRING";
        case TransferState::VERIFYING: return "VERIFYING";
        case TransferState::COMMITTING: return "COMMITTING";
        case TransferState::COMPLETED: return "COMPLETED";
        case TransferState::PAUSED: return "PAUSED";
        case TransferState::RETRYING: return "RETRYING";
        case TransferState::REPAIRING: return "REPAIRING";
        case TransferState::CANCELLED: return "CANCELLED";
        case TransferState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

static std::string chunk_state_to_string(ChunkState state) {
    switch(state) {
        case ChunkState::PENDING: return "PENDING";
        case ChunkState::IN_PROGRESS: return "IN_PROGRESS";
        case ChunkState::COMPLETE: return "COMPLETE";
        case ChunkState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

static TransferState string_to_state(const std::string& str) {
    if (str == "CREATED") return TransferState::CREATED;
    if (str == "AUTHENTICATING") return TransferState::AUTHENTICATING;
    if (str == "DISCOVERING") return TransferState::DISCOVERING;
    if (str == "NEGOTIATING") return TransferState::NEGOTIATING;
    if (str == "PLANNING") return TransferState::PLANNING;
    if (str == "TRANSFERRING") return TransferState::TRANSFERRING;
    if (str == "VERIFYING") return TransferState::VERIFYING;
    if (str == "COMMITTING") return TransferState::COMMITTING;
    if (str == "COMPLETED") return TransferState::COMPLETED;
    if (str == "PAUSED") return TransferState::PAUSED;
    if (str == "RETRYING") return TransferState::RETRYING;
    if (str == "REPAIRING") return TransferState::REPAIRING;
    if (str == "CANCELLED") return TransferState::CANCELLED;
    if (str == "FAILED") return TransferState::FAILED;
    return TransferState::FAILED;
}

static ChunkState string_to_chunk_state(const std::string& str) {
    if (str == "PENDING") return ChunkState::PENDING;
    if (str == "IN_PROGRESS") return ChunkState::IN_PROGRESS;
    if (str == "COMPLETE") return ChunkState::COMPLETE;
    return ChunkState::FAILED;
}

static ResourceInfo parse_resource_uri(const std::string& uri) {
    ResourceInfo info;
    auto pos = uri.find("://");
    if (pos != std::string::npos) {
        info.provider = uri.substr(0, pos);
        info.path = uri.substr(pos + 3);
    } else {
        info.provider = "local";
        info.path = uri;
    }
    return info;
}

StateManager::StateManager(const std::string& db_path) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
    }
    sqlite3_busy_timeout(db_, 10000);
    execute("PRAGMA journal_mode=WAL;");
    init_schema();
}

StateManager::~StateManager() {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    if (db_) {
        sqlite3_close(db_);
    }
}

void StateManager::execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        if (err_msg) sqlite3_free(err_msg);
        throw std::runtime_error("SQLite execution failed: " + err);
    }
}

void StateManager::init_schema() {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS transfers (
            id TEXT PRIMARY KEY,
            source_uri TEXT NOT NULL,
            destination_uri TEXT NOT NULL,
            state TEXT NOT NULL,
            bytes_total INTEGER NOT NULL,
            bytes_completed INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS chunks (
            transfer_id TEXT NOT NULL,
            object_id TEXT,
            chunk_index INTEGER NOT NULL,
            offset INTEGER NOT NULL,
            size INTEGER NOT NULL,
            state TEXT NOT NULL,
            checksum TEXT,
            attempts INTEGER DEFAULT 0,
            PRIMARY KEY (transfer_id, chunk_index)
        );
        CREATE TABLE IF NOT EXISTS sessions (
            transfer_id TEXT NOT NULL,
            role TEXT NOT NULL,
            token TEXT NOT NULL,
            updated_at INTEGER NOT NULL,
            PRIMARY KEY (transfer_id, role)
        );
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            transfer_id TEXT NOT NULL,
            event_type TEXT NOT NULL,
            payload TEXT NOT NULL DEFAULT '',
            created_at INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_events_transfer ON events(transfer_id, id);
    )";
    execute(schema);

    // Lightweight migration: existing databases predate these columns.
    add_column_if_missing("transfers", "chunk_size",
        "ALTER TABLE transfers ADD COLUMN chunk_size INTEGER NOT NULL DEFAULT 0");
    add_column_if_missing("transfers", "control",
        "ALTER TABLE transfers ADD COLUMN control TEXT NOT NULL DEFAULT ''");
    add_column_if_missing("transfers", "last_error",
        "ALTER TABLE transfers ADD COLUMN last_error TEXT NOT NULL DEFAULT ''");
    add_column_if_missing("transfers", "http_code",
        "ALTER TABLE transfers ADD COLUMN http_code INTEGER NOT NULL DEFAULT 0");
}

void StateManager::add_column_if_missing(const std::string& table, const std::string& column, const std::string& ddl) {
    std::string pragma = "PRAGMA table_info(" + table + ")";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, pragma.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        if (name && reinterpret_cast<const char*>(name) == column) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (!found) {
        try {
            execute(ddl);
        } catch (const std::exception&) {
            // Another process/thread added it first — harmless duplicate-column error.
        }
    }
}

void StateManager::create_transfer(const Transfer& transfer) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    // P2.4: upsert, not INSERT OR REPLACE — a re-submitted transfer with the
    // same id must keep its completed-chunk progress (resume semantics), and
    // only reset when the payload actually changed. Zero-size placeholder
    // creates (state persisted before stat()) never clobber a real row.
    const char* sql =
        "INSERT INTO transfers (id, source_uri, destination_uri, state, bytes_total, bytes_completed, chunk_size, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  source_uri = excluded.source_uri, "
        "  destination_uri = excluded.destination_uri, "
        "  state = excluded.state, "
        "  bytes_total = CASE WHEN excluded.bytes_total > 0 THEN excluded.bytes_total ELSE transfers.bytes_total END, "
        "  bytes_completed = CASE WHEN excluded.bytes_total > 0 AND transfers.bytes_total != excluded.bytes_total THEN 0 ELSE transfers.bytes_completed END, "
        "  chunk_size = CASE WHEN excluded.chunk_size > 0 THEN excluded.chunk_size ELSE transfers.chunk_size END, "
        "  updated_at = excluded.updated_at";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    auto now = std::chrono::system_clock::now().time_since_epoch().count();

    sqlite3_bind_text(stmt, 1, transfer.id.c_str(), -1, SQLITE_TRANSIENT);
    std::string source_uri = transfer.source.provider + "://" + transfer.source.path;
    std::string dest_uri = transfer.destination.provider + "://" + transfer.destination.path;
    sqlite3_bind_text(stmt, 2, source_uri.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, dest_uri.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, state_to_string(transfer.state).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, transfer.size);
    sqlite3_bind_int64(stmt, 6, transfer.bytes_transferred);
    sqlite3_bind_int64(stmt, 7, transfer.chunk_size);
    sqlite3_bind_int64(stmt, 8, now);
    sqlite3_bind_int64(stmt, 9, now);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute insert statement");
    }
    sqlite3_finalize(stmt);
}

void StateManager::update_transfer_state(const std::string& transfer_id, TransferState state) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET state = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    
    sqlite3_bind_text(stmt, 1, state_to_string(state).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute update state statement");
    }
    sqlite3_finalize(stmt);
}

void StateManager::update_transfer_progress(const std::string& transfer_id, uint64_t bytes_completed) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET bytes_completed = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    
    sqlite3_bind_int64(stmt, 1, bytes_completed);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute update progress statement");
    }
    sqlite3_finalize(stmt);
}

std::optional<Transfer> StateManager::get_transfer(const std::string& transfer_id) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "SELECT source_uri, destination_uri, state, bytes_total, bytes_completed, chunk_size FROM transfers WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare select transfer statement");
    }

    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<Transfer> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Transfer t;
        t.id = transfer_id;
        t.source = parse_resource_uri(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        t.destination = parse_resource_uri(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        t.state = string_to_state(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        t.size = sqlite3_column_int64(stmt, 3);
        t.bytes_transferred = sqlite3_column_int64(stmt, 4);
        t.chunk_size = sqlite3_column_int64(stmt, 5);
        result = t;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Transfer> StateManager::list_transfers(int limit) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    std::string sql = "SELECT id, source_uri, destination_uri, state, bytes_total, bytes_completed FROM transfers ORDER BY created_at DESC LIMIT " + std::to_string(limit);
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare select transfers statement");
    }

    std::vector<Transfer> list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Transfer t;
        t.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        t.source = parse_resource_uri(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        t.destination = parse_resource_uri(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        t.state = string_to_state(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        t.size = sqlite3_column_int64(stmt, 4);
        t.bytes_transferred = sqlite3_column_int64(stmt, 5);
        list.push_back(t);
    }

    sqlite3_finalize(stmt);
    return list;
}

std::vector<ChunkInfo> StateManager::get_plan(const std::string& transfer_id) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "SELECT offset, size, state, attempts FROM chunks WHERE transfer_id = ? ORDER BY chunk_index ASC";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare select chunks statement");
    }

    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<ChunkInfo> plan;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChunkInfo c;
        c.offset = sqlite3_column_int64(stmt, 0);
        c.size = sqlite3_column_int64(stmt, 1);
        c.state = string_to_chunk_state(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        c.attempts = static_cast<uint32_t>(sqlite3_column_int64(stmt, 3));
        plan.push_back(c);
    }

    sqlite3_finalize(stmt);
    return plan;
}

void StateManager::save_plan(const std::string& transfer_id, const std::vector<ChunkInfo>& plan) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    execute("BEGIN TRANSACTION;");
    try {
        // P2.4: upsert, not INSERT OR REPLACE. Re-planning an identical file
        // keeps every COMPLETE chunk and its attempt history (this is what
        // makes re-submitted transfers resumable); a changed file (different
        // offsets/sizes) resets the affected chunks to PENDING instead of
        // letting stale offsets corrupt the resume.
        const char* sql =
            "INSERT INTO chunks (transfer_id, chunk_index, offset, size, state, attempts) "
            "VALUES (?, ?, ?, ?, ?, 0) "
            "ON CONFLICT(transfer_id, chunk_index) DO UPDATE SET "
            "  offset = excluded.offset, "
            "  size = excluded.size, "
            "  state = CASE WHEN chunks.offset != excluded.offset OR chunks.size != excluded.size "
            "               THEN 'PENDING' ELSE chunks.state END, "
            "  attempts = CASE WHEN chunks.offset != excluded.offset OR chunks.size != excluded.size "
            "               THEN 0 ELSE chunks.attempts END";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }

        for (size_t i = 0; i < plan.size(); ++i) {
            sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, i);
            sqlite3_bind_int64(stmt, 3, plan[i].offset);
            sqlite3_bind_int64(stmt, 4, plan[i].size);
            sqlite3_bind_text(stmt, 5, chunk_state_to_string(plan[i].state).c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                throw std::runtime_error("Failed to execute insert chunk statement");
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);

        // Drop stale tail chunks if the new plan is shorter than the old one.
        const char* prune =
            "DELETE FROM chunks WHERE transfer_id = ? AND chunk_index >= ?";
        if (sqlite3_prepare_v2(db_, prune, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare prune statement");
        }
        sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(plan.size()));
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to execute prune statement");
        }
        sqlite3_finalize(stmt);

        execute("COMMIT;");
    } catch (...) {
        execute("ROLLBACK;");
        throw;
    }
}

void StateManager::update_chunk_state(const std::string& transfer_id, uint64_t chunk_index, ChunkState state) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE chunks SET state = ? WHERE transfer_id = ? AND chunk_index = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, chunk_state_to_string(state).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, chunk_index);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute update chunk state statement");
    }
    sqlite3_finalize(stmt);
}

// ─────────────────────────────────────────────────────────────────────────────
// P1.3: upload-session tokens — the engine persists the provider-opaque token
// on openWrite and passes it back on resume so the same upload continues.
// ─────────────────────────────────────────────────────────────────────────────

void StateManager::save_session_token(const std::string& transfer_id, const std::string& role, const std::string& token) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "INSERT OR REPLACE INTO sessions (transfer_id, role, token, updated_at) VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare session token insert");
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, now);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute session token insert");
    }
    sqlite3_finalize(stmt);
}

std::string StateManager::get_session_token(const std::string& transfer_id, const std::string& role) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "SELECT token FROM sessions WHERE transfer_id = ? AND role = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_TRANSIENT);
    std::string token;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(stmt, 0);
        if (t) token = reinterpret_cast<const char*>(t);
    }
    sqlite3_finalize(stmt);
    return token;
}

void StateManager::delete_session_tokens(const std::string& transfer_id) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "DELETE FROM sessions WHERE transfer_id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.2/P2.4: error paths persist what actually happened (§ errors.md).
// ─────────────────────────────────────────────────────────────────────────────

void StateManager::update_transfer_size(const std::string& transfer_id, uint64_t bytes_total) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET bytes_total = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare size update");
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    sqlite3_bind_int64(stmt, 1, bytes_total);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute size update");
    }
    sqlite3_finalize(stmt);
}

void StateManager::record_transfer_error(const std::string& transfer_id, const std::string& error, int http_code) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET last_error = ?, http_code = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return; // best effort — never mask the original failure
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    sqlite3_bind_text(stmt, 1, error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, http_code);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_text(stmt, 4, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void StateManager::record_chunk_attempt(const std::string& transfer_id, uint64_t chunk_index) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE chunks SET attempts = attempts + 1 WHERE transfer_id = ? AND chunk_index = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return; // best effort
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, chunk_index);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.1: event ledger
// ─────────────────────────────────────────────────────────────────────────────

void StateManager::log_event(const std::string& transfer_id, EventType type, const std::string& payload) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "INSERT INTO events (transfer_id, event_type, payload, created_at) VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return; // an event-logging failure must never kill the transfer
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, event_type_to_string(type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Event> StateManager::get_events(const std::string& transfer_id, int limit) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "SELECT event_type, payload, created_at FROM events WHERE transfer_id = ? ORDER BY id DESC LIMIT ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {};
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    std::vector<Event> events;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Event ev;
        ev.transfer_id = transfer_id;
        const unsigned char* type_c = sqlite3_column_text(stmt, 0);
        ev.type = event_type_from_string(reinterpret_cast<const char*>(type_c));
        const unsigned char* payload_c = sqlite3_column_text(stmt, 1);
        if (payload_c) ev.payload = reinterpret_cast<const char*>(payload_c);
        ev.timestamp = sqlite3_column_int64(stmt, 2);
        events.push_back(std::move(ev));
    }
    sqlite3_finalize(stmt);

    // Queried newest-first; return chronologically.
    std::reverse(events.begin(), events.end());
    return events;
}

// ─────────────────────────────────────────────────────────────────────────────
// P2.3: cross-process control plane
// ─────────────────────────────────────────────────────────────────────────────

void StateManager::request_control(const std::string& transfer_id, const std::string& action) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET control = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare control statement");
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    sqlite3_bind_text(stmt, 1, action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to set control flag");
    }
    sqlite3_finalize(stmt);
}

std::string StateManager::get_control(const std::string& transfer_id) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "SELECT control FROM transfers WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    std::string control;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* c = sqlite3_column_text(stmt, 0);
        if (c) control = reinterpret_cast<const char*>(c);
    }
    sqlite3_finalize(stmt);
    return control;
}

void StateManager::clear_control(const std::string& transfer_id) {
    std::lock_guard<std::recursive_mutex> lock(g_db_mutex);
    const char* sql = "UPDATE transfers SET control = '' WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, transfer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

} // namespace utp
