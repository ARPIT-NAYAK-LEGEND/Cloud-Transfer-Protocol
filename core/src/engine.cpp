#include "utp/engine.hpp"
#include "utp/chunk_planner.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>

namespace utp {

std::string Engine::generate_id() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return "utp_" + std::to_string(now);
}

Engine::Engine(std::unique_ptr<StorageProvider> source, std::unique_ptr<StorageProvider> dest, const std::string& db_path)
    : source_(std::move(source)), dest_(std::move(dest)), state_manager_(db_path) {}

void Engine::transfer(const Path& src_path, const Path& dest_path) {
    auto stat_info = source_->stat(src_path);
    auto size = stat_info.size;

    Transfer transfer_state;
    transfer_state.id = generate_id();
    transfer_state.source = {"local", src_path};
    transfer_state.destination = {"local", dest_path};
    transfer_state.size = size;
    transfer_state.state = TransferState::CREATED;
    transfer_state.bytes_transferred = 0;
    
    auto source_caps = source_->capabilities();
    auto dest_caps = dest_->capabilities();

    auto plan = ChunkPlanner::plan(size, source_caps, dest_caps);
    
    // In chunk planner it determines chunk size, let's just pick the size of the first chunk if plan is not empty
    transfer_state.chunk_size = plan.empty() ? 0 : plan[0].size;
    
    state_manager_.create_transfer(transfer_state);
    state_manager_.update_transfer_state(transfer_state.id, TransferState::PLANNING);
    
    state_manager_.save_plan(transfer_state.id, plan);

    state_manager_.update_transfer_state(transfer_state.id, TransferState::TRANSFERRING);

    auto read_session = source_->openRead(src_path);
    auto write_session = dest_->openWrite(dest_path, size);

    // Bounded buffer memory discipline
    size_t max_chunk_size = transfer_state.chunk_size;
    for (const auto& c : plan) {
        if (c.size > max_chunk_size) max_chunk_size = c.size;
    }
    std::vector<uint8_t> buffer(max_chunk_size);

    for (size_t i = 0; i < plan.size(); ++i) {
        auto& chunk = plan[i];
        state_manager_.update_chunk_state(transfer_state.id, i, ChunkState::IN_PROGRESS);

        size_t bytes_read = source_->read(read_session, buffer.data(), chunk.size);
        if (bytes_read == 0 && chunk.size > 0) {
            state_manager_.update_chunk_state(transfer_state.id, i, ChunkState::FAILED);
            state_manager_.update_transfer_state(transfer_state.id, TransferState::FAILED);
            dest_->abort(write_session);
            return;
        }

        dest_->write(write_session, chunk.offset, buffer.data(), bytes_read);
        
        state_manager_.update_chunk_state(transfer_state.id, i, ChunkState::COMPLETE);
        transfer_state.bytes_transferred += bytes_read;
        state_manager_.update_transfer_progress(transfer_state.id, transfer_state.bytes_transferred);
    }

    dest_->commit(write_session);
    state_manager_.update_transfer_state(transfer_state.id, TransferState::COMPLETED);
}

} // namespace utp
