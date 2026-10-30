// ═══════════════════════════════════════════════════════════════════════════════
//  SURFACE STATUS: POST-MVP EXPERIMENTAL DAEMON (utpd)
//  This REST API server is an experimental post-MVP surface pending multi-cloud
//  DoD automation. Production MVP transfers should run through the direct CLI / TUI.
// ═══════════════════════════════════════════════════════════════════════════════

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
#include <random>
#include <iomanip>
#include <fstream>
#include <cstring>

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
        std::string cid = env_id ? env_id : "617211477359-8a1s4v0aamr860vrs2k9bn9h6lou534u.apps.googleusercontent.com";
        std::string csec = env_sec ? env_sec : "";
        return std::make_unique<GoogleDriveProvider>(cid, csec, effective_context);
    }
    if (base_scheme == "onedrive") {
        const char* env_id = std::getenv("ONEDRIVE_CLIENT_ID");
        const char* env_sec = std::getenv("ONEDRIVE_CLIENT_SECRET");
        std::string cid = env_id ? env_id : "5091c9e9-674d-466c-849a-e1bf7d6badee";
        std::string csec = env_sec ? env_sec : "";
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

static std::string resolve_local_resource_path(const Resource& res) {
    if (res.path.empty()) {
        return res.host_or_bucket;
    }
    if (!res.host_or_bucket.empty()) {
        return res.host_or_bucket + "/" + res.path;
    }
#ifdef _WIN32
    if (res.path.length() >= 2 && res.path[1] == ':') {
        return res.path;
    }
    return "/" + res.path;
#else
    if (!res.path.empty() && res.path[0] != '/') {
        return "/" + res.path;
    }
    return res.path;
#endif
}

static std::string get_resource_path(const Resource& res) {
    if (res.scheme == "file" || res.scheme == "local") {
        return resolve_local_resource_path(res);
    }
    if (res.path.empty()) {
        return res.host_or_bucket;
    }
    return res.host_or_bucket.empty() ? res.path : (res.host_or_bucket + "/" + res.path);
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

static std::string get_or_create_api_token() {
    const char* env_token = std::getenv("UTP_API_TOKEN");
    if (env_token && std::strlen(env_token) > 0) {
        std::string s(env_token);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
        if (!s.empty()) return s;
    }

    const std::string token_file = ".utp_api_token";
    std::ifstream infile(token_file);
    if (infile.is_open()) {
        std::string token;
        if (std::getline(infile, token)) {
            while (!token.empty() && (token.back() == '\r' || token.back() == '\n' || token.back() == ' ')) token.pop_back();
            if (!token.empty()) return token;
        }
    }

    std::random_device rd;
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    std::stringstream ss;
    for (int i = 0; i < 4; ++i) {
        ss << std::hex << std::setw(8) << std::setfill('0') << dist(rd);
    }
    std::string new_token = ss.str();

    std::ofstream outfile(token_file);
    if (outfile.is_open()) {
        outfile << new_token << "\n";
    }
    return new_token;
}

int main() {
    Server svr;
    StateManager sm("utp.db");
    
    // Global mutex to protect engine execution
    std::mutex engine_mutex;

    std::string api_token = get_or_create_api_token();
    std::cout << "[UTP Daemon] API Token: " << api_token << "\n";
    std::cout << "[UTP Daemon] Bearer authentication required on all /api/* routes\n";

    // P3: Reject unauthenticated API calls
    svr.set_pre_routing_handler([api_token](const Request& req, Response& res) {
        if (req.method == "OPTIONS") {
            return Server::HandlerResponse::Unhandled;
        }

        if (req.path.rfind("/api/", 0) == 0) {
            std::string auth_header = req.get_header_value("Authorization");
            std::string provided_token;
            if (auth_header.rfind("Bearer ", 0) == 0) {
                provided_token = auth_header.substr(7);
            } else if (req.has_header("X-API-Key")) {
                provided_token = req.get_header_value("X-API-Key");
            }
            while (!provided_token.empty() && (provided_token.back() == ' ' || provided_token.back() == '\r' || provided_token.back() == '\n')) provided_token.pop_back();
            while (!provided_token.empty() && (provided_token.front() == ' ' || provided_token.front() == '\t')) provided_token.erase(0, 1);

            if (provided_token.empty() || provided_token != api_token) {
                res.status = 401;
                res.set_header("WWW-Authenticate", "Bearer realm=\"utp\"");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("{\"error\": \"Unauthorized: missing or invalid API token\"}", "application/json");
                return Server::HandlerResponse::Handled;
            }
        }

        return Server::HandlerResponse::Unhandled;
    });

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

        // P1.4: keep the account alias with the scheme so the engine persists
        // the full provider identity and `utp resume` finds the right account.
        if (!src_res.account.empty()) src_scheme += ":" + src_res.account;
        if (!dest_res.account.empty()) dest_scheme += ":" + dest_res.account;
        
        std::string src_path = get_resource_path(src_res);
        std::string dest_path = get_resource_path(dest_res);

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

    // GET /api/transfers/:id/events — the §7 lifecycle event stream from the
    // ledger; CLI and Web consume the same stream instead of ad-hoc state.
    svr.Get(R"(/api/transfers/([a-zA-Z0-9_-]+)/events)", [&](const Request& req, Response& res) {
        std::string id = req.matches[1];
        auto opt_t = sm.get_transfer(id);
        if (!opt_t) {
            res.status = 404;
            res.set_content("{\"error\": \"Transfer not found\"}", "application/json");
            return;
        }
        auto events = sm.get_events(id, 200);
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < events.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{";
            ss << "\"transfer_id\": \"" << events[i].transfer_id << "\",";
            ss << "\"type\": \"" << event_type_to_string(events[i].type) << "\",";
            ss << "\"payload\": \"" << events[i].payload << "\",";
            ss << "\"timestamp\": " << events[i].timestamp;
            ss << "}";
        }
        ss << "]";
        res.set_content(ss.str(), "application/json");
    });

    // POST /api/transfers/:id/pause | /cancel — writes the ledger control flag
    // a running engine polls between chunks (P2.3).
    svr.Post(R"(/api/transfers/([a-zA-Z0-9_-]+)/(pause|cancel))", [&](const Request& req, Response& res) {
        std::string id = req.matches[1];
        std::string action = req.matches[2];
        auto opt_t = sm.get_transfer(id);
        if (!opt_t) {
            res.status = 404;
            res.set_content("{\"error\": \"Transfer not found\"}", "application/json");
            return;
        }
        if (opt_t->state == TransferState::COMPLETED || opt_t->state == TransferState::CANCELLED) {
            res.status = 409;
            res.set_content("{\"error\": \"Transfer already in terminal state: " + state_to_string(opt_t->state) + "\"}", "application/json");
            return;
        }
        sm.request_control(id, action);
        res.status = 202;
        std::stringstream ss;
        ss << "{\"status\": \"requested\", \"action\": \"" << action << "\", \"id\": \"" << id << "\"}";
        res.set_content(ss.str(), "application/json");
    });

    // Enable CORS for Web UI
    svr.Options(R"(/api/.*)", [](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
        res.status = 200;
    });

    svr.set_post_routing_handler([](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("X-UTP-Surface", "experimental");
    });

    std::cout << "[UTP Daemon] API Server running on http://0.0.0.0:8080 (EXPERIMENTAL: Post-MVP surface pending Phase 4 gate verification)\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
