#include "utp/uri_parser.hpp"
#include "utp/engine.hpp"
#include "providers/local/local_provider.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace utp;

int main(int argc, char* argv[]) {
    if (argc != 4 || std::string(argv[1]) != "transfer") {
        std::cerr << "Usage: utp transfer <src> <dest>\n";
        return 1;
    }

    try {
        Resource src_res = parse_uri(argv[2]);
        Resource dest_res = parse_uri(argv[3]);

        if (src_res.scheme != "local" || dest_res.scheme != "local") {
            std::cerr << "Error: Only local:// to local:// transfers are supported in this milestone.\n";
            return 1;
        }

        std::unique_ptr<StorageProvider> src_provider = std::make_unique<LocalProvider>();
        std::unique_ptr<StorageProvider> dest_provider = std::make_unique<LocalProvider>();

        // Reconstruct absolute paths for local:// URIs
        std::string src_path = (src_res.host_or_bucket.empty() ? "/" : "") + src_res.path;
        std::string dest_path = (dest_res.host_or_bucket.empty() ? "/" : "") + dest_res.path;

        Engine engine(std::move(src_provider), std::move(dest_provider));
        engine.transfer(src_path, dest_path);

        std::cout << "Transfer complete!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
