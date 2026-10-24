#include "httplib.h"
#include "utp/uri_parser.hpp"
#include "utp/engine.hpp"
#include "utp/state_manager.hpp"
#include "providers/local/local_provider.hpp"
#include "providers/google_drive/gdrive_provider.hpp"
#include "providers/onedrive/onedrive_provider.hpp"
#include "providers/dropbox/dropbox_provider.hpp"
#include "providers/s3/s3_provider.hpp"
#include "providers/azure/azure_provider.hpp"
#include "providers/webdav/webdav_provider.hpp"
#include "providers/icloud/icloud_provider.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <sstream>
#include <cstdlib>

using namespace httplib;
using namespace utp;

std::unique_ptr<StorageProvider> create_provider(const std::string& scheme) {
    std::string base_scheme = scheme;
    std::string effective_context = "";
    size_t d = scheme.find_first_of(":+");
    if (d != std::string::npos) {
        base_scheme = scheme.substr(0, d);
        effective_context = scheme.substr(d + 1);
    }

    if (base_scheme == "local" || base_scheme == "file") return std::make_unique<LocalProvider>();
    if (base_scheme == "gdrive") {
        const char* env_id = std::getenv("GDRIVE_CLIENT_ID");
        const char* env_sec = std::getenv("GDRIVE_CLIENT_SECRET");
        static const char redacted_g[] = "REDACTED_XOR_SECRET";
        std::string cid = env_id ? env_id : "617211477359-8a1s4v0aamr860vrs2k9bn9h6lou534u.apps.googleusercontent.com";
        std::string csec;
        if (env_sec) {
            csec = env_sec;
        } else {
            csec.resize(sizeof(redacted_g) - 1);
            for (size_t i = 0; i < sizeof(redacted_g) - 1; ++i) csec[i] = static_cast<char>(static_cast<uint8_t>(redacted_g[i]) ^ 0x42);
        }
        return std::make_unique<GoogleDriveProvider>(cid, csec, effective_context);
    }
    if (base_scheme == "onedrive") {
        const char* env_id = std::getenv("ONEDRIVE_CLIENT_ID");
        const char* env_sec = std::getenv("ONEDRIVE_CLIENT_SECRET");
        static const char redacted_o[] = "REDACTED_XOR_SECRET";
        std::string cid = env_id ? env_id : "5091c9e9-674d-466c-849a-e1bf7d6badee";
        std::string csec;
        if (env_sec) {
            csec = env_sec;
        } else {
            csec.resize(sizeof(redacted_o) - 1);
            for (size_t i = 0; i < sizeof(redacted_o) - 1; ++i) csec[i] = static_cast<char>(static_cast<uint8_t>(redacted_o[i]) ^ 0x42);
        }
        return std::make_unique<OneDriveProvider>(cid, csec, effective_context);
    }
    if (base_scheme == "dropbox") return std::make_unique<DropboxProvider>("", "", effective_context);
    if (base_scheme == "s3") return std::make_unique<S3Provider>();
    if (base_scheme == "r2") {
        std::string endpoint = "";
        const char* ep = std::getenv("R2_ENDPOINT");
        if (ep) endpoint = ep;
        else {
            const char* acct = std::getenv("R2_ACCOUNT_ID");
            if (!acct) acct = std::getenv("CLOUDFLARE_ACCOUNT_ID");
            if (acct) endpoint = "https://" + std::string(acct) + ".r2.cloudflarestorage.com";
        }
        const char* key = std::getenv("R2_ACCESS_KEY_ID");
        if (!key) key = std::getenv("AWS_ACCESS_KEY_ID");
        const char* secret = std::getenv("R2_SECRET_ACCESS_KEY");
        if (!secret) secret = std::getenv("AWS_SECRET_ACCESS_KEY");
        return std::make_unique<S3Provider>(key ? key : "", secret ? secret : "", "auto", endpoint);
    }
    if (base_scheme == "b2") {
        std::string endpoint = "";
        const char* ep = std::getenv("B2_ENDPOINT");
        if (ep) endpoint = ep;
        else {
            const char* reg = std::getenv("B2_REGION");
            std::string region_str = reg ? reg : "us-west-004";
            endpoint = "https://s3." + region_str + ".backblazeb2.com";
        }
        const char* key = std::getenv("B2_APPLICATION_KEY_ID");
        if (!key) key = std::getenv("AWS_ACCESS_KEY_ID");
        const char* secret = std::getenv("B2_APPLICATION_KEY");
        if (!secret) secret = std::getenv("AWS_SECRET_ACCESS_KEY");
        const char* reg = std::getenv("B2_REGION");
        return std::make_unique<S3Provider>(key ? key : "", secret ? secret : "", reg ? reg : "us-west-004", endpoint);
    }
    if (base_scheme == "webdav" || base_scheme == "nextcloud" || base_scheme == "owncloud") {
        return std::make_unique<WebDAVProvider>("", "", "", effective_context);
    }
    if (base_scheme == "icloud") return std::make_unique<ICloudProvider>("", "", effective_context);
    if (base_scheme == "azure") return std::make_unique<AzureBlobProvider>();
    throw std::runtime_error("Unsupported provider scheme: " + base_scheme);
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

int main() {
    Server svr;
    StateManager sm("utp.db");
    
    // Global mutex to protect engine execution
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
        
        std::string src_path = (src_res.host_or_bucket.empty() ? "" : (src_res.host_or_bucket + "/")) + src_res.path;
        std::string dest_path = (dest_res.host_or_bucket.empty() ? "" : (dest_res.host_or_bucket + "/")) + dest_res.path;

        try {
            auto src_provider = create_provider(src_scheme);
            auto dest_provider = create_provider(dest_scheme);
            
            auto engine = std::make_shared<Engine>(std::move(src_provider), std::move(dest_provider));
            std::string transfer_id = Engine::generate_id();

            // Persist initial CREATED state
            Transfer init_t;
            init_t.id = transfer_id;
            init_t.source = {src_scheme, src_path};
            init_t.destination = {dest_scheme, dest_path};
            init_t.state = TransferState::CREATED;
            init_t.size = 0;
            init_t.bytes_transferred = 0;
            sm.create_transfer(init_t);
            
            std::thread([engine = engine, src_scheme = src_scheme, src_path = src_path, dest_scheme = dest_scheme, dest_path = dest_path, transfer_id = transfer_id, &engine_mutex]() {
                std::lock_guard<std::mutex> lock(engine_mutex);
                try {
                    engine->transfer(src_scheme, src_path, dest_scheme, dest_path, nullptr, transfer_id);
                } catch (const std::exception& e) {
                    std::cerr << "Background transfer error: " << e.what() << "\n";
                }
            }).detach();

            res.status = 202;
            std::stringstream ss;
            ss << "{\"status\": \"accepted\", \"id\": \"" << transfer_id << "\", \"message\": \"Transfer initiated in background.\"}";
            res.set_content(ss.str(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\": \"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // GET /api/transfers/:id
    svr.Get(R"(/api/transfers/([a-zA-Z0-9_-]+))", [&](const Request& req, Response& res) {
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
        ss << "\"source\": \"" << opt_t->source.provider << "://" << opt_t->source.path << "\",";
        ss << "\"destination\": \"" << opt_t->destination.provider << "://" << opt_t->destination.path << "\",";
        ss << "\"state\": \"" << state_to_string(opt_t->state) << "\",";
        ss << "\"size\": " << opt_t->size << ",";
        ss << "\"bytes_transferred\": " << opt_t->bytes_transferred;
        ss << "}";

        res.set_content(ss.str(), "application/json");
    });
    
    // GET /api/transfers (List all)
    svr.Get("/api/transfers", [&](const Request& /*req*/, Response& res) {
        auto list = sm.list_transfers(50);
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{";
            ss << "\"id\": \"" << list[i].id << "\",";
            ss << "\"source\": \"" << list[i].source.provider << "://" << list[i].source.path << "\",";
            ss << "\"destination\": \"" << list[i].destination.provider << "://" << list[i].destination.path << "\",";
            ss << "\"state\": \"" << state_to_string(list[i].state) << "\",";
            ss << "\"size\": " << list[i].size << ",";
            ss << "\"bytes_transferred\": " << list[i].bytes_transferred;
            ss << "}";
        }
        ss << "]";
        res.set_content(ss.str(), "application/json");
    });

    // Enable CORS for Web UI
    svr.Options("/api/transfers", [](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    svr.set_post_routing_handler([](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    std::cout << "[UTP Daemon] API Server running on http://0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
