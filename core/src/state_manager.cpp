#include "utp/state_manager.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>
#include <chrono>

namespace utp {

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

StateManager::StateManager(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
    }
    init_schema();
}

StateManager::~StateManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void StateManager::execute(const std::string& sql) {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "Unknown error";
        if (err_msg) sqlite3_free(err_msg);
        throw std::runtime_error("SQLite execution failed: " + err);
    }
}

void StateManager::init_schema() {
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
    )";
    execute(schema);
}

void StateManager::create_transfer(const Transfer& transfer) {
    const char* sql = "INSERT OR REPLACE INTO transfers (id, source_uri, destination_uri, state, bytes_total, bytes_completed, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
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
    sqlite3_bind_int64(stmt, 7, now);
    sqlite3_bind_int64(stmt, 8, now);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute insert statement");
    }
    sqlite3_finalize(stmt);
}

void StateManager::update_transfer_state(const std::string& transfer_id, TransferState state) {
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

void StateManager::save_plan(const std::string& transfer_id, const std::vector<ChunkInfo>& plan) {
    execute("BEGIN TRANSACTION;");
    try {
        const char* sql = "INSERT OR REPLACE INTO chunks (transfer_id, chunk_index, offset, size, state) VALUES (?, ?, ?, ?, ?)";
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
        execute("COMMIT;");
    } catch (...) {
        execute("ROLLBACK;");
        throw;
    }
}

void StateManager::update_chunk_state(const std::string& transfer_id, uint64_t chunk_index, ChunkState state) {
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

} // namespace utp
