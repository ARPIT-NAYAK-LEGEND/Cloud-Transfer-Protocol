#pragma once

#include "providers/interface/storage_provider.hpp"
#include "utp/state_manager.hpp"
#include <memory>
#include <string>

namespace utp {

class Engine {
public:
    Engine(std::unique_ptr<StorageProvider> source, std::unique_ptr<StorageProvider> dest, const std::string& db_path = "utp.db");

    void transfer(const Path& src_path, const Path& dest_path);

private:
    std::unique_ptr<StorageProvider> source_;
    std::unique_ptr<StorageProvider> dest_;
    StateManager state_manager_;
    std::string generate_id();
};

} // namespace utp
