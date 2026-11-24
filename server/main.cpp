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
#include "providers/http/http_provider.hpp"
#include "providers/common/credentials.hpp"
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
        std::string cid = utp::resolve_gdrive_client_id();
        std::string csec = utp::resolve_gdrive_client_secret(cid);
        return std::make_unique<GoogleDriveProvider>(cid, csec, effective_context);
    }
    if (base_scheme == "onedrive") {
        std::string cid = utp::resolve_onedrive_client_id();
        std::string csec = utp::resolve_onedrive_client_secret(cid);
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
    if (base_scheme == "http" || base_scheme == "https") return std::make_unique<HttpProvider>("", effective_context);
    throw std::runtime_error("Unsupported provider scheme: " + base_scheme);
}

// Media MIME type detector for zero-disk HTTP Range streaming
static std::string get_media_mime_type(const std::string& filename) {
    std::string ext = "";
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) {
        ext = filename.substr(dot);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".mp4" || ext == ".m4v") return "video/mp4";
    if (ext == ".mkv") return "video/x-matroska";
    if (ext == ".webm") return "video/webm";
    if (ext == ".avi") return "video/x-msvideo";
    if (ext == ".mov") return "video/quicktime";
    if (ext == ".wmv") return "video/x-ms-wmv";
    if (ext == ".flv") return "video/x-flv";
    if (ext == ".ts") return "video/mp2t";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".flac") return "audio/flac";
    if (ext == ".wav") return "audio/wav";
    if (ext == ".m4a" || ext == ".aac") return "audio/mp4";
    if (ext == ".ogg" || ext == ".oga") return "audio/ogg";
    if (ext == ".pdf") return "application/pdf";
    return "application/octet-stream";
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

int main(int argc, char* argv[]) {
    int port = 8080;
    const char* port_env = std::getenv("UTP_PORT");
    if (!port_env) port_env = std::getenv("PORT");
    if (port_env) {
        try { port = std::stoi(port_env); } catch (...) {}
    }
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            try { port = std::stoi(argv[++i]); } catch (...) {}
        } else if (std::isdigit(static_cast<unsigned char>(a[0]))) {
            try { port = std::stoi(a); } catch (...) {}
        }
    }

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
            } else if (req.has_param("token")) {
                provided_token = req.get_param_value("token");
            } else if (req.has_param("api_key")) {
                provided_token = req.get_param_value("api_key");
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

    // ═══════════════════════════════════════════════════════════════════════════
    //  ZERO-DISK CLOUD STREAMING REST ENDPOINT (HTTP 206 RANGE & SEEKING)
    // ═══════════════════════════════════════════════════════════════════════════
    auto serve_stream = [&](const std::string& raw_uri, const Request& req, Response& res) {
        if (raw_uri.empty()) {
            res.status = 400;
            res.set_content("{\"error\": \"Missing URI parameter\"}", "application/json");
            return;
        }

        Resource res_info;
        try {
            res_info = parse_uri(raw_uri);
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid cloud URI: " + std::string(e.what()) + "\"}", "application/json");
            return;
        }

        std::string scheme = res_info.scheme;
        if (scheme == "https" && res_info.host_or_bucket.find("drive.google.com") != std::string::npos) scheme = "gdrive";
        if (!res_info.account.empty()) scheme += ":" + res_info.account;

        std::string remote_path = get_resource_path(res_info);

        std::shared_ptr<StorageProvider> provider;
        try {
            provider = create_provider(scheme);
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\": \"Failed to initialize provider: " + std::string(e.what()) + "\"}", "application/json");
            return;
        }

        std::string filename = "";
        uint64_t file_size = 0;
        try {
            FileInfo fi = provider->stat(remote_path);
            filename = fi.name.empty() ? remote_path : fi.name;
            file_size = fi.size;
        } catch (const std::exception& e) {
            size_t slash = remote_path.find_last_of("/\\");
            filename = (slash != std::string::npos) ? remote_path.substr(slash + 1) : remote_path;
        }

        if (req.has_param("name") && !req.get_param_value("name").empty()) {
            filename = req.get_param_value("name");
        }
        if (filename.empty()) filename = "stream.mp4";

        std::string mime = get_media_mime_type(filename);

        ReadStream rstream;
        try {
            rstream = provider->openRead(remote_path);
        } catch (const std::exception& e) {
            res.status = 502;
            res.set_content("{\"error\": \"Failed to open remote stream: " + std::string(e.what()) + "\"}", "application/json");
            return;
        }

        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Range, Accept, Content-Type, Authorization, X-API-Key");
        res.set_header("Access-Control-Expose-Headers", "Content-Range, Content-Length, Accept-Ranges");

        bool download_mode = (req.has_param("download") && (req.get_param_value("download") == "1" || req.get_param_value("download") == "true"));
        std::string disposition = download_mode ? "attachment" : "inline";
        res.set_header("Content-Disposition", disposition + "; filename=\"" + filename + "\"");

        res.set_content_provider(
            file_size,
            mime,
            [provider, rstream, file_size](size_t offset, size_t length, DataSink& sink) -> bool {
                const size_t CHUNK = 512 * 1024; // 512 KB volatile chunk in RAM
                size_t remaining = length;
                size_t cur_offset = offset;
                std::vector<uint8_t> buffer(CHUNK);

                while (remaining > 0) {
                    size_t to_read = std::min(remaining, CHUNK);
                    size_t got = 0;
                    try {
                        got = provider->read(rstream, cur_offset, buffer.data(), to_read);
                    } catch (...) {
                        return false;
                    }
                    if (got == 0) break;
                    if (!sink.write(reinterpret_cast<const char*>(buffer.data()), got)) {
                        return false; // Connection closed or seek scrubbed by client
                    }
                    cur_offset += got;
                    remaining -= got;
                }
                return true;
            }
        );
    };

    // GET & HEAD /api/stream?uri=... or ?src=... or ?id=...
    svr.Get("/api/stream", [&](const Request& req, Response& res) {
        std::string target_uri;
        if (req.has_param("uri")) target_uri = req.get_param_value("uri");
        else if (req.has_param("src")) target_uri = req.get_param_value("src");
        else if (req.has_param("id")) {
            auto opt_t = sm.get_transfer(req.get_param_value("id"));
            if (!opt_t) {
                res.status = 404;
                res.set_content("{\"error\": \"Transfer ID not found\"}", "application/json");
                return;
            }
            target_uri = opt_t->source.provider + "://" + opt_t->source.path;
        }

        if (target_uri.empty()) {
            res.status = 400;
            res.set_content("{\"error\": \"Missing required 'uri' or 'src' or 'id' query parameter\"}", "application/json");
            return;
        }

        serve_stream(target_uri, req, res);
    });

    svr.Get(R"(/api/stream/([a-zA-Z0-9_-]+))", [&](const Request& req, Response& res) {
        std::string id = req.matches[1];
        auto opt_t = sm.get_transfer(id);
        if (!opt_t) {
            res.status = 404;
            res.set_content("{\"error\": \"Transfer not found\"}", "application/json");
            return;
        }
        std::string target_uri = opt_t->source.provider + "://" + opt_t->source.path;
        serve_stream(target_uri, req, res);
    });

    // Enable CORS for Web UI
    svr.Options(R"(/api/.*)", [](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, HEAD");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key, Range, Accept");
        res.set_header("Access-Control-Expose-Headers", "Content-Range, Content-Length, Accept-Ranges");
        res.status = 200;
    });

    svr.set_post_routing_handler([](const Request& /*req*/, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("X-UTP-Surface", "experimental");
    });

    std::cout << "[UTP Daemon] API Server running on http://0.0.0.0:" << port << " (EXPERIMENTAL: Post-MVP surface pending Phase 4 gate verification)\n";
    std::cout << "[UTP Daemon] svr.is_valid(): " << svr.is_valid() << "\n";
    bool ret = svr.listen("0.0.0.0", port);
    std::cout << "[UTP Daemon] svr.listen() returned: " << ret << "\n";
    return 0;
}
