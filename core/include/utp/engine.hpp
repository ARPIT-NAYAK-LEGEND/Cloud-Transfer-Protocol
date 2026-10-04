#pragma once

#include "providers/interface/storage_provider.hpp"
#include <memory>

namespace utp {

class Engine {
public:
    Engine(std::unique_ptr<StorageProvider> source, std::unique_ptr<StorageProvider> dest);

    void transfer(const Path& src_path, const Path& dest_path);

private:
    std::unique_ptr<StorageProvider> source_;
    std::unique_ptr<StorageProvider> dest_;
};

} // namespace utp
