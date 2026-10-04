#include "utp/engine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

namespace utp {

Engine::Engine(std::unique_ptr<StorageProvider> source, std::unique_ptr<StorageProvider> dest)
    : source_(std::move(source)), dest_(std::move(dest)) {}

void Engine::transfer(const Path& src_path, const Path& dest_path) {
    auto stat_info = source_->stat(src_path);
    auto size = stat_info.size;

    auto read_session = source_->openRead(src_path);
    auto write_session = dest_->openWrite(dest_path, size);

    constexpr size_t BUFFER_SIZE = 8 * 1024 * 1024; // 8 MB bounded buffer
    std::vector<uint8_t> buffer(BUFFER_SIZE);

    uint64_t offset = 0;
    while (offset < size) {
        size_t to_read = std::min(static_cast<uint64_t>(BUFFER_SIZE), size - offset);
        size_t bytes_read = source_->read(read_session, buffer.data(), to_read);
        
        if (bytes_read == 0) {
            break; // EOF or error
        }

        dest_->write(write_session, offset, buffer.data(), bytes_read);
        offset += bytes_read;
    }

    dest_->commit(write_session);
}

} // namespace utp
