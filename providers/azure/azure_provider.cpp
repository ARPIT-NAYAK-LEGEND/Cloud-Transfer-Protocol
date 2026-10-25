#include "azure_provider.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace utp {

AzureBlobProvider::AzureBlobProvider() {
    account_name_ = "mock_azure_account";
    account_key_ = "mock_azure_key";
    const char* env_acc = std::getenv("AZURE_STORAGE_ACCOUNT");
    if (env_acc && strlen(env_acc) > 0) account_name_ = env_acc;
    const char* env_key = std::getenv("AZURE_STORAGE_KEY");
    if (env_key && strlen(env_key) > 0) account_key_ = env_key;
    endpoint_ = "https://" + account_name_ + ".blob.core.windows.net";
    const char* env_ep = std::getenv("AZURE_STORAGE_ENDPOINT");
    if (env_ep && strlen(env_ep) > 0) endpoint_ = env_ep;
}

AzureBlobProvider::~AzureBlobProvider() {}

std::string AzureBlobProvider::sign_shared_key(const std::string& method, const std::string& url, const std::vector<std::string>& headers) {
    // TODO: Implement SharedKey HMAC-SHA256
    return "mock_azure_signature";
}

ProviderCapabilities AzureBlobProvider::capabilities() {
    return {
        true,  // ranged_read
        false, // resumable_upload (Azure uses Put Block / Put Block List)
        true,  // multipart_upload
        true,  // server_side_copy
        true,  // checksum (MD5)
        true,  // metadata
        false, // directory_support (Blob is flat)
        4000ULL * 1024 * 1024, // max_chunk_size (4000 MB for block blob limit)
        16     // max_parallel_requests
    };
}

FileInfo AzureBlobProvider::stat(Path path) {
    std::vector<std::string> headers = {
        "x-ms-version: 2020-10-02",
        "Authorization: " + sign_shared_key("HEAD", endpoint_ + "/" + path, {})
    };
    auto resp = http_.get(endpoint_ + "/" + path, headers); // HEAD stub
    if (resp.status_code != 200) {
        throw TransferError(ErrorCategory::FATAL, "Azure stat failed: " + resp.body);
    }
    return {path, 1024 * 1024 * 10, false}; // mock 10MB
}

std::vector<FileInfo> AzureBlobProvider::list(Path path) {
    return {};
}

ReadStream AzureBlobProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "azure_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, {}};
    return {id};
}

WriteSession AzureBlobProvider::openWrite(Path path, uint64_t size, const std::string& resume_token) {
    (void)resume_token; // TODO(Phase 3): persist block-list state for Azure resume

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "azure_write_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, {}};
    return {id}; // Azure doesn't require init for Put Block
}

size_t AzureBlobProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.find(session.id) == sessions_.end()) {
        throw TransferError(ErrorCategory::FATAL, "Invalid Azure read session");
    }
    std::memset(buffer, 0, count);
    return count;
}

void AzureBlobProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    Session* s = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid Azure write session");
        }
        s = &sessions_[session.id];
    }

    uint32_t block_id_num = static_cast<uint32_t>(offset / (4 * 1024 * 1024)) + 1; // 4MB chunks
    std::stringstream ss;
    ss << std::setw(6) << std::setfill('0') << block_id_num;
    std::string block_id_str = ss.str();
    
    // Base64 encode the block id (Mocked here since we just need a unique ID)
    std::string block_id_b64 = "YmxvY2tf" + block_id_str; 

    std::string url = endpoint_ + "/" + s->path + "?comp=block&blockid=" + block_id_b64;
    std::vector<std::string> headers = {
        "x-ms-version: 2020-10-02",
        "Authorization: " + sign_shared_key("PUT", url, {})
    };

    auto resp = http_.put(url, data, size, headers);
    
    if (resp.status_code != 201) {
        if (resp.status_code == 403) throw TransferError(ErrorCategory::AUTH, "Azure auth error", resp.status_code);
        if (resp.status_code == 503) throw TransferError(ErrorCategory::RATE_LIMIT, "Azure slow down", resp.status_code);
        throw TransferError(ErrorCategory::RETRYABLE, "Azure Put Block failed: " + resp.body, resp.status_code);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        s->block_ids[block_id_num] = block_id_b64;
    }
}

void AzureBlobProvider::commit(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session.id);
    if (it == sessions_.end()) return;
    
    std::string url = endpoint_ + "/" + it->second.path + "?comp=blocklist";
    std::vector<std::string> headers = {
        "x-ms-version: 2020-10-02",
        "Authorization: " + sign_shared_key("PUT", url, {})
    };
    
    // Mock the XML body
    std::string body = "<?xml version=\"1.0\" encoding=\"utf-8\"?><BlockList>";
    for (auto const& [num, b64] : it->second.block_ids) {
        body += "<Latest>" + b64 + "</Latest>";
    }
    body += "</BlockList>";

    auto resp = http_.put(url, reinterpret_cast<const uint8_t*>(body.c_str()), body.length(), headers);
    if (resp.status_code != 201) {
        throw TransferError(ErrorCategory::FATAL, "Azure Put Block List failed: " + resp.body);
    }

    sessions_.erase(it);
}

void AzureBlobProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

} // namespace utp
