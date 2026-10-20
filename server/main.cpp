#include "httplib.h"
#include "utp/uri_parser.hpp"
#include "utp/engine.hpp"
#include "utp/state_manager.hpp"
#include "providers/local/local_provider.hpp"
#include "providers/google_drive/gdrive_provider.hpp"
#include "providers/onedrive/onedrive_provider.hpp"
#include "providers/s3/s3_provider.hpp"
#include "providers/azure/azure_provider.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <sstream>

using namespace httplib;
using namespace utp;

std::unique_ptr<StorageProvider> create_provider(const std::string& scheme) {
    if (scheme == "local" || scheme == "file") return std::make_unique<LocalProvider>();
    if (scheme == "gdrive") return std::make_unique<GoogleDriveProvider>();
    if (scheme == "onedrive") return std::make_unique<OneDriveProvider>();
    if (scheme == "s3") return std::make_unique<S3Provider>();
    if (scheme == "azure") return std::make_unique<AzureBlobProvider>();
    throw std::runtime_error("Unsupported provider scheme: " + scheme);
}

// Minimal JSON extractor for {"src": "...", "dest": "..."}
static std::string extract_json_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
    size_t end = pos;
    while (end < json.length() && json[end] != '"' && json[end] != ',' && json[end] != '}' && json[end] != ' ' && json[end] != '\r' && json[end] != '\n') end++;
    return json.substr(pos, end - pos);
}

// Convert TransferState enum to string
std::string state_to_string(TransferState s) {
    switch(s) {
        case TransferState::CREATED: return "CREATED";
        case TransferState::PLANNING: return "PLANNING";
        case TransferState::TRANSFERRING: return "TRANSFERRING";
        case TransferState::VERIFYING: return "VERIFYING";
        case TransferState::COMMITTING: return "COMMITTING";
        case TransferState::COMPLETED: return "COMPLETED";
        case TransferState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

int main() {
    Server svr;
    StateManager sm("utp.db");
    
    // Global mutex to protect engine execution (MVP constraint to avoid colliding db transactions)
    std::mutex engine_mutex;

    // POST /api/transfers
    svr.Post("/api/transfers", [&](const Request& req, Response& res) {
        std::string src_uri = extract_json_value(req.body, "src");
        std::string dest_uri = extract_json_value(req.body, "dest");

        if (src_uri.empty() || dest_uri.empty()) {
            res.status = 400;
            res.set_content("{\"error\": \"Missing src or dest URI\"}", "application/json");
            return;
        }

        Resource src_res = parse_uri(src_uri);
        Resource dest_res = parse_uri(dest_uri);
        
        std::string src_scheme = src_res.scheme;
        if (src_scheme == "https" && src_res.host_or_bucket.find("drive.google.com") != std::string::npos) src_scheme = "gdrive";
        
        std::string dest_scheme = dest_res.scheme;
        if (dest_scheme == "https" && dest_res.host_or_bucket.find("drive.google.com") != std::string::npos) dest_scheme = "gdrive";
        
        std::string src_path = (src_res.host_or_bucket.empty() ? "/" : "") + src_res.path;
        std::string dest_path = (dest_res.host_or_bucket.empty() ? "/" : "") + dest_res.path;

        try {
            auto src_provider = create_provider(src_scheme);
            auto dest_provider = create_provider(dest_scheme);
            
            // We must instantiate the engine dynamically
            auto engine = std::make_shared<Engine>(std::move(src_provider), std::move(dest_provider));
            
            // Start transfer synchronously to get the ID, then we should technically run it async.
            // Wait, we need the ID immediately. We will extract ID generation to an async wrap if possible,
            // but for MVP, we can run transfer async and rely on Engine to create it quickly.
            // Actually, we'll run it in a detached thread. To get the ID, we'll manually generate it here 
            // or let the thread do it. Since `transfer()` creates the ID, we'll just run it synchronously 
            // for the MVP demo, or just use `resume` logic.
            // Let's modify: we will run `transfer()` in a thread, but wait until the ID is created?
            // Easier: just let `transfer` block for this MVP daemon, or return immediately with a "started" status.
            
            std::thread([engine = engine, src_scheme = src_scheme, src_path = src_path, dest_scheme = dest_scheme, dest_path = dest_path, &engine_mutex]() {
                std::lock_guard<std::mutex> lock(engine_mutex);
                try {
                    engine->transfer(src_scheme, src_path, dest_scheme, dest_path, nullptr);
                } catch (const std::exception& e) {
                    std::cerr << "Background transfer error: " << e.what() << "\n";
                }
            }).detach();

            res.status = 202;
            res.set_content("{\"status\": \"accepted\", \"message\": \"Transfer initiated in background. Query GET /api/transfers to poll status.\"}", "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // GET /api/transfers/:id
    svr.Get(R"(/api/transfers/(utp_[0-9]+))", [&](const Request& req, Response& res) {
        std::string id = req.matches[1];
        auto opt_t = sm.get_transfer(id);
        
        if (!opt_t) {
            res.status = 404;
            res.set_content("{\"error\": \"Transfer not found\"}", "application/json");
            return;
        }

        std::stringstream ss;
        ss << "{";
        ss << "\"id\": \"" << opt_t->id << "\",";
        ss << "\"state\": \"" << state_to_string(opt_t->state) << "\",";
        ss << "\"size\": " << opt_t->size << ",";
        ss << "\"bytes_transferred\": " << opt_t->bytes_transferred;
        ss << "}";

        res.set_content(ss.str(), "application/json");
    });
    
    // GET /api/transfers (List all)
    svr.Get("/api/transfers", [&](const Request& req, Response& res) {
        // Since we don't have a list_all in StateManager yet, we'll just return a stub
        res.set_content("{\"message\": \"Use /api/transfers/<id> to poll a specific transfer.\"}", "application/json");
    });

    // Enable CORS for Web UI
    svr.Options("/api/transfers", [](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    svr.set_post_routing_handler([](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    std::cout << "[UTP Daemon] API Server running on http://0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}

// Server graceful shutdown handler
