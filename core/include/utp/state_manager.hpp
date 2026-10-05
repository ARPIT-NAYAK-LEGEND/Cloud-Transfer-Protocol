#pragma once

#include "utp/types.hpp"
#include <string>
#include <vector>
#include <memory>

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

    void save_plan(const std::string& transfer_id, const std::vector<ChunkInfo>& plan);
    void update_chunk_state(const std::string& transfer_id, uint64_t chunk_index, ChunkState state);

private:
    sqlite3* db_{nullptr};

    void execute(const std::string& sql);
};

} // namespace utp
