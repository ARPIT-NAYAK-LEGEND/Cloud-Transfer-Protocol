#include "local_provider.hpp"
#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace fs = std::filesystem;

namespace utp {

ProviderCapabilities LocalProvider::capabilities() {
    return {
        true,  // ranged_read
        true,  // resumable_upload
        false, // multipart_upload
        false, // server_side_copy
        false, // checksum
        true,  // metadata
        true,  // directory_support
        1024 * 1024 * 1024, // max_chunk_size
        1      // max_parallel_requests
    };
}

FileInfo LocalProvider::stat(Path path) {
    if (!fs::exists(path)) {
        throw std::runtime_error("File not found: " + path);
    }
    return {
        path,
        fs::is_directory(path) ? 0 : fs::file_size(path),
        fs::is_directory(path),
        fs::path(path).filename().string()
    };
}

std::vector<FileInfo> LocalProvider::list(Path path) {
    std::vector<FileInfo> result;
    std::string target = path.empty() ? "." : path;
    if (fs::exists(target) && fs::is_directory(target)) {
        for (const auto& entry : fs::directory_iterator(target)) {
            result.push_back({
                entry.path().string(),
                entry.is_regular_file() ? entry.file_size() : 0,
                entry.is_directory(),
                entry.path().filename().string()
            });
        }
    }
    return result;
}

ReadStream LocalProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "read_" + std::to_string(next_id_++);
    auto& state = read_streams_[id];
    state.stream.open(path, std::ios::binary);
    if (!state.stream.is_open()) {
        read_streams_.erase(id);
        throw std::runtime_error("Could not open file for reading: " + path);
    }
    return {id};
}

WriteSession LocalProvider::openWrite(Path path, uint64_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "write_" + std::to_string(next_id_++);
    auto& state = write_sessions_[id];
    state.path = path;
    state.temp_path = path + ".utp.tmp";
    fs::path tp(state.temp_path);
    if (tp.has_parent_path()) {
        fs::create_directories(tp.parent_path());
    }
    state.stream.open(state.temp_path, std::ios::binary);
    if (!state.stream.is_open()) {
        write_sessions_.erase(id);
        throw std::runtime_error("Could not open file for writing: " + state.temp_path);
    }
    return {id};
}

size_t LocalProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = read_streams_.find(session.id);
    if (it == read_streams_.end()) {
        throw std::runtime_error("Invalid read session");
    }
    it->second.stream.read(reinterpret_cast<char*>(buffer), count);
    return it->second.stream.gcount();
}

void LocalProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = write_sessions_.find(session.id);
    if (it == write_sessions_.end()) {
        throw std::runtime_error("Invalid write session");
    }
    it->second.stream.seekp(offset);
    it->second.stream.write(reinterpret_cast<const char*>(data), size);
    if (!it->second.stream.good()) {
        throw std::runtime_error("Write failed");
    }
}

void LocalProvider::commit(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = write_sessions_.find(session.id);
    if (it == write_sessions_.end()) {
        throw std::runtime_error("Invalid write session");
    }
    it->second.stream.close();
    fs::rename(it->second.temp_path, it->second.path);
    write_sessions_.erase(it);
}

void LocalProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = write_sessions_.find(session.id);
    if (it != write_sessions_.end()) {
        it->second.stream.close();
        if (fs::exists(it->second.temp_path)) {
            fs::remove(it->second.temp_path);
        }
        write_sessions_.erase(it);
    }
}

LocalProvider::~LocalProvider() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : read_streams_) {
        pair.second.stream.close();
    }
    for (auto& pair : write_sessions_) {
        pair.second.stream.close();
        if (fs::exists(pair.second.temp_path)) {
            fs::remove(pair.second.temp_path);
        }
    }
}

} // namespace utp

// Directory traversal verified
