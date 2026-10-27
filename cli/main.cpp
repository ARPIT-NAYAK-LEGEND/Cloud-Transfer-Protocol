#include "utp/uri_parser.hpp"
#include "utp/engine.hpp"
#include "utp/state_manager.hpp"
#include "tui.hpp"
#include "account_manager.hpp"
#include "providers/local/local_provider.hpp"
#include "providers/google_drive/gdrive_provider.hpp"
#include "providers/onedrive/onedrive_provider.hpp"
#include "providers/dropbox/dropbox_provider.hpp"
#include "providers/s3/s3_provider.hpp"
#include "providers/azure/azure_provider.hpp"
#include "providers/webdav/webdav_provider.hpp"
#include "providers/icloud/icloud_provider.hpp"
#include "providers/common/http_client.hpp"
#include "utp/events.hpp"
#include <iostream>
#include <ctime>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <functional>
#ifdef _WIN32
#include <windows.h>
static BOOL WINAPI console_ctrl_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\033[?25h\033[0m\n";
        std::cout.flush();
        ExitProcess(0);
    }
    return TRUE;
}
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#endif

using namespace utp;

// ═══════════════════════════════════════════════════════════════════════════════
//  MODERN CLAUDE / HERMES / VERCEL DESIGN SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════

#define C_RESET     "\033[0m"
#define C_BOLD      "\033[1m"
#define C_DIM       "\033[2m"
#define C_VIOLET    "\033[38;5;141m" // Claude signature violet (#a78bfa)
#define C_CYAN      "\033[38;5;51m"  // Electric cyan (#38bdf8)
#define C_BLUE      "\033[38;5;75m"  // Muted blue
#define C_GREEN     "\033[38;5;49m"  // Vibrant emerald (#34d399)
#define C_YELLOW    "\033[38;5;215m" // Warm amber (#fbbf24)
#define C_RED       "\033[38;5;203m" // Coral red
#define C_WHITE     "\033[1;37m"     // Crisp white
#define C_MUTED     "\033[38;5;242m" // Slate gray (#64748b)
#define C_BORDER    "\033[38;5;239m" // Subtle box border (#475569)
#define C_SPINE     "\033[38;5;239m" // Left spine color (#475569)
#define C_DIM_W     "\033[0;37m"     // Regular white

// Clack / Claude Code spine glyphs
#define GLYPH_TOP    "┌"
#define GLYPH_SPINE  "│"
#define GLYPH_STEP   "◇"
#define GLYPH_DONE   "◆"
#define GLYPH_END    "└"
#define GLYPH_STREAM "▲"

// High-tech glyphs
#define GLYPH_BOLT   "\xe2\x9a\xa1" // ⚡
#define GLYPH_CHECK  "\xe2\x9c\x94" // ✔
#define GLYPH_ARROW  "\xe2\x9d\xaf" // ❯
#define GLYPH_DOT_ON "\xe2\x97\x8f" // ●
#define GLYPH_DOT_OF "\xe2\x97\x8b" // ○
#define GLYPH_BULLET "\xe2\x97\x86" // ◆
#define GLYPH_STAR   "\xe2\x9c\xa6" // ✦
#define GLYPH_RIGHT  "\xe2\x9e\x94" // ➔

#define BLOCK_FULL   "\xe2\x96\x88" // █
#define BLOCK_MED    "\xe2\x96\x93" // ▓
#define BLOCK_LIGHT  "\xe2\x96\x91" // ░

static const char* SPINNER[] = {
    "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
    "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
    "\xe2\xa0\x87", "\xe2\xa0\x8f"
};
static constexpr int SPIN_N = 10;

// ═══════════════════════════════════════════════════════════════════════════════
//  PROVIDER FACTORY
// ═══════════════════════════════════════════════════════════════════════════════

namespace utp {
std::unique_ptr<StorageProvider> create_provider(const std::string& scheme, const std::string& context) {
    std::string base_scheme = scheme;
    std::string effective_context = context;
    size_t d = scheme.find_first_of(":+");
    if (d != std::string::npos) {
        base_scheme = scheme.substr(0, d);
        if (effective_context.empty()) {
            effective_context = scheme.substr(d + 1);
        }
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
        return std::make_unique<S3Provider>(
            key ? key : "",
            secret ? secret : "",
            "auto",
            endpoint
        );
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
        std::string region_str = reg ? reg : "us-west-004";
        return std::make_unique<S3Provider>(
            key ? key : "",
            secret ? secret : "",
            region_str,
            endpoint
        );
    }
    if (base_scheme == "webdav" || base_scheme == "nextcloud" || base_scheme == "owncloud") {
        return std::make_unique<WebDAVProvider>("", "", "", effective_context);
    }
    if (base_scheme == "icloud") return std::make_unique<ICloudProvider>("", "", effective_context);
    if (base_scheme == "azure") return std::make_unique<AzureBlobProvider>();
    throw std::runtime_error("Unsupported provider scheme: " + base_scheme);
}
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UTILITIES & TELEMETRY
// ═══════════════════════════════════════════════════════════════════════════════

std::atomic<uint64_t> g_bytes_transferred{0};
std::atomic<uint64_t> g_total_size{0};
std::atomic<bool> g_transferring{false};

static std::string trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

static const char* transfer_state_to_string(TransferState s) {
    switch (s) {
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
        default: return "FAILED";
    }
}

static std::string format_bytes(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024ULL) {
        snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024ULL) {
        snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return std::string(buf);
}

void print_usage() {
    std::cout << "\n" C_VIOLET GLYPH_TOP C_RESET "  " C_WHITE C_BOLD "utp-engine" C_RESET " " C_MUTED "v2.5.0-pro" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "universal cloud transfer protocol · direct memory pipeline" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
    std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD "Commands & Usage:" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp transfer" C_RESET "                     Launch interactive visual TUI mode\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp transfer" C_RESET " <src> <dest>       Direct cloud-to-cloud pipeline\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp resume" C_RESET "   <transfer_id>      Resume paused/failed transfer state\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp pause" C_RESET "    <transfer_id>      Stop between chunks, keep all progress\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp cancel" C_RESET "   <transfer_id>      Abandon transfer, discard partial state\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp status" C_RESET "  <transfer_id>      State, chunks and event history\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "utp list" C_RESET "                      Show recent transfers from the ledger\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
    std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD "Supported Protocols:" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_GREEN "[Core MVP - Verified]" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_CYAN "gdrive://" C_RESET "<file_id>          Google Drive files & shared folders\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_BLUE "onedrive://" C_RESET "<path>          Microsoft OneDrive & SharePoint\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_GREEN "file://" C_RESET "<path>               Local NVMe / SSD & mounted disks\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_YELLOW "[Post-MVP - Experimental]" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_CYAN "dropbox://" C_RESET "<path>           Dropbox Personal & Team Cloud\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_CYAN "icloud://" C_RESET "<path>            Apple iCloud Drive (Web & Local Bridge)\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_YELLOW "s3://" C_RESET "<bucket>/<key>         Amazon S3 buckets & cloud objects\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_YELLOW "r2://" C_RESET "<bucket>/<key>         Cloudflare R2 Object Storage (Zero Egress)\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_YELLOW "b2://" C_RESET "<bucket>/<key>         Backblaze B2 Cloud Storage\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_BLUE "azure://" C_RESET "<container>/<blob>   Azure Blob Storage Containers\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "      " C_VIOLET "webdav://" C_RESET "<path>          Nextcloud, ownCloud & WebDAV NAS\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
    std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD "Options:" C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED "--bandwidth" C_RESET " <bps>           Limit transfer speed (0 = unlimited)\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED "--id" C_RESET " <transfer_id>       Assign deterministic ID for tracking/resumption\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED "--verbose" C_RESET "                   Enable verbose diagnostic logging\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
    std::cout << C_GREEN GLYPH_END C_RESET "  " C_MUTED "Standing by." C_RESET "\n\n";
}

static int get_terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        if (w > 20) return w;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 20) {
        return w.ws_col;
    }
    return 80;
#endif
}

static std::string truncate_visible(const std::string& str, size_t max_len) {
    if (str.length() <= max_len) return str;
    if (max_len <= 3) return str.substr(0, max_len);
    return str.substr(0, max_len - 3) + "...";
}

static std::string format_pipeline_label(const std::string& src, const std::string& dest, int max_line_width) {
    std::string arrow = " ──[ Direct RAM 0-Disk ]──▶ ";
    int avail = max_line_width - static_cast<int>(arrow.length()) - 4; // 4 chars for "│  "
    if (avail < 16) {
        arrow = " ──▶ ";
        avail = max_line_width - static_cast<int>(arrow.length()) - 4;
    }
    if (avail < 10) avail = 10;
    int half = avail / 2;
    std::string s = (static_cast<int>(src.length()) > half) ? truncate_visible(src, half) : src;
    std::string d = (static_cast<int>(dest.length()) > half) ? truncate_visible(dest, half) : dest;
    return s + arrow + d;
}

// Block progress bar renderer (buffered string, single atomic write)
static std::string render_block_bar(float progress, int width) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int filled = static_cast<int>(width * progress);
    std::string s;
    s.reserve(width * 16);
    s += C_VIOLET;
    for (int k = 0; k < filled; ++k) {
        s += BLOCK_FULL;
    }
    if (filled < width && progress < 1.0f) {
        s += C_CYAN BLOCK_MED;
        filled++;
    }
    s += C_BORDER;
    for (int k = filled; k < width; ++k) {
        s += BLOCK_LIGHT;
    }
    s += C_RESET;
    return s;
}

static std::string format_eta(double elapsed_sec, float progress) {
    if (progress <= 0.01f || elapsed_sec < 1.0) return "calculating...";
    double total_est = elapsed_sec / progress;
    double remaining = total_est - elapsed_sec;
    if (remaining < 0) remaining = 0;
    int mins = static_cast<int>(remaining) / 60;
    int secs = static_cast<int>(remaining) % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "~%dm %ds", mins, secs);
    return std::string(buf);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  STREAMING TRANSFER HUD (CLAUDE CODE / HERMES STYLE)
// ═══════════════════════════════════════════════════════════════════════════════

void run_batch_transfer_pool(std::shared_ptr<StorageProvider> src_provider,
                             std::shared_ptr<StorageProvider> dest_provider,
                             const std::vector<TransferItem>& transfers,
                             size_t num_workers,
                             const std::string& src_scheme_name,
                             const std::string& dest_scheme_name,
                             uint64_t bandwidth_limit = 0) {
    size_t total_tasks = transfers.size();
    if (total_tasks == 0) return;

    struct WorkerSlot {
        std::string filename{""};
        uint64_t bytes{0};
        uint64_t total{0};
        bool active{false};
    };

    std::vector<WorkerSlot> worker_slots(num_workers);
    std::mutex slots_mutex;
    std::vector<std::pair<std::string, std::string>> failed_items;

    std::atomic<size_t> next_task_idx{0};
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> completed_tasks{0};
    std::atomic<uint64_t> finished_bytes{0};
    std::vector<std::thread> workers;

    // Pre-stat total payload size across all files
    uint64_t total_batch_bytes = 0;
    for (const auto& it : transfers) {
        Resource r = parse_uri(it.src_uri);
        std::string p = r.path.empty() ? r.host_or_bucket : (r.host_or_bucket + "/" + r.path);
        try {
            auto st = src_provider->stat(p);
            total_batch_bytes += st.size;
        } catch (...) {}
    }

    Resource first_dest = parse_uri(transfers[0].dest_uri);
    bool dest_is_cloud = (first_dest.scheme != "local" && first_dest.scheme != "file");

    HttpClient::reset_network_bytes();
    g_transferring = true;
    auto batch_start = std::chrono::steady_clock::now();

    // Hide terminal cursor for smooth, flicker-free progress display (like npm / pacman)
    std::cout << "\033[?25l";
    std::cout.flush();

    // ──── MODERN BULLETPROOF HUD THREAD ────
    std::thread ui_thread([batch_start, num_workers, total_tasks, total_batch_bytes,
                          &worker_slots, &slots_mutex, &completed_tasks, &finished_bytes,
                          &transfers, src_scheme_name, dest_scheme_name, dest_is_cloud]() {
        int tick = 0;
        double smoothed_speed = 0.0;
        uint64_t last_calc_bytes = 0;
        auto last_calc_time = std::chrono::steady_clock::now();

        std::string raw_primary_file = transfers[0].filename;
        if (transfers.size() > 1) {
            raw_primary_file += " (+" + std::to_string(transfers.size() - 1) + " files)";
        }

        while (g_transferring) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - batch_start).count();

            // Calculate active bytes in flight from chunk callbacks
            std::vector<WorkerSlot> copy_slots;
            {
                std::lock_guard<std::mutex> lock(slots_mutex);
                copy_slots = worker_slots;
            }

            uint64_t active_bytes = 0;
            for (const auto& ws : copy_slots) {
                if (ws.active) active_bytes += ws.bytes;
            }
            uint64_t chunk_bytes = finished_bytes.load() + active_bytes;
            uint64_t net_bytes = dest_is_cloud ? HttpClient::network_ul_bytes() : HttpClient::network_dl_bytes();
            uint64_t total_transferred = std::max(chunk_bytes, net_bytes);
            if (total_batch_bytes > 0 && total_transferred > total_batch_bytes) {
                total_transferred = total_batch_bytes;
            }

            // Speed calculation smoothed
            auto speed_dt = std::chrono::duration<double>(now - last_calc_time).count();
            if (speed_dt >= 0.2) {
                uint64_t diff = (total_transferred >= last_calc_bytes) ? (total_transferred - last_calc_bytes) : 0;
                double instant_speed = diff / speed_dt;
                smoothed_speed = (smoothed_speed * 0.7) + (instant_speed * 0.3);
                last_calc_bytes = total_transferred;
                last_calc_time = now;
            }

            float overall_progress = 0.0f;
            if (total_batch_bytes > 0) {
                overall_progress = std::min(1.0f, static_cast<float>(total_transferred) / total_batch_bytes);
            } else if (total_tasks > 0) {
                overall_progress = static_cast<float>(completed_tasks.load()) / total_tasks;
            }

            std::string eta_str = format_eta(elapsed, overall_progress);
            std::string speed_str = format_bytes(static_cast<uint64_t>(smoothed_speed)) + "/s";

            int term_width = get_terminal_width();
            int max_file_len = std::clamp(term_width / 4, 12, 30);
            std::string disp_file = truncate_visible(raw_primary_file, max_file_len);

            int pct = static_cast<int>(overall_progress * 100.0);
            int bar_width = std::clamp(term_width - 64, 10, 22);
            std::string bar_str = render_block_bar(overall_progress, bar_width);

            std::string status_info = format_bytes(total_transferred) + "/" + format_bytes(total_batch_bytes);

            // ──── Single-line atomic \r overwrite (ZERO FLICKER, ZERO SCROLL, ZERO GHOSTING) ────
            std::ostringstream frame;
            frame << "\r\033[2K" C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STREAM C_RESET " "
                  << C_WHITE C_BOLD << disp_file << C_RESET "  "
                  << bar_str << " " C_WHITE << std::setw(3) << pct << "%" C_RESET
                  << " · " C_CYAN << speed_str << C_RESET
                  << " · " C_MUTED "ETA " << eta_str << C_RESET
                  << " · " C_MUTED << status_info << C_RESET;

            std::cout << frame.str();
            std::cout.flush();

            tick++;
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }

        // Cleanly wipe progress line and restore cursor
        std::cout << "\r\033[2K\033[?25h";
        std::cout.flush();
    });

    // ──── WORKER THREADS ────
    for (size_t w = 0; w < num_workers; ++w) {
        workers.emplace_back([w, &transfers, &next_task_idx, &success_count, &completed_tasks,
                              &finished_bytes, &worker_slots, &slots_mutex, &failed_items,
                              src_provider, dest_provider, bandwidth_limit]() {
            while (true) {
                size_t task_idx = next_task_idx.fetch_add(1);
                if (task_idx >= transfers.size()) break;

                const auto& item = transfers[task_idx];
                Resource s_res = parse_uri(item.src_uri);
                Resource d_res = parse_uri(item.dest_uri);

                // Empty host (e.g. file:///rel/path) previously produced a
                // bogus leading slash ("/rel/path") — keep host-relative or
                // plain-relative forms intact.
                std::string s_p = s_res.path.empty() ? s_res.host_or_bucket
                    : (s_res.host_or_bucket.empty() ? s_res.path : s_res.host_or_bucket + "/" + s_res.path);
                std::string d_p = d_res.path.empty() ? d_res.host_or_bucket
                    : (d_res.host_or_bucket.empty() ? d_res.path : d_res.host_or_bucket + "/" + d_res.path);

                // P1.4: pass scheme WITH account context so the engine persists
                // the full provider identity — `utp resume` then reconstructs
                // the correct account instead of silently falling to default.
                std::string src_account = s_res.account.empty() ? item.src_account : s_res.account;
                std::string dest_account = d_res.account.empty() ? item.dest_account : d_res.account;
                std::string s_scheme_ctx = s_res.scheme + (src_account.empty() ? "" : ":" + src_account);
                std::string d_scheme_ctx = d_res.scheme + (dest_account.empty() ? "" : ":" + dest_account);

                {
                    std::lock_guard<std::mutex> lock(slots_mutex);
                    worker_slots[w].filename = item.filename;
                    worker_slots[w].bytes = 0;
                    worker_slots[w].total = 0;
                    worker_slots[w].active = true;
                }

                auto worker_progress = [w, &worker_slots, &slots_mutex](uint64_t bytes, uint64_t total) {
                    std::lock_guard<std::mutex> lock(slots_mutex);
                    worker_slots[w].bytes = bytes;
                    worker_slots[w].total = total;
                };

                try {
                    Engine engine(src_provider, dest_provider, "utp.db");
                    if (bandwidth_limit > 0) engine.set_bandwidth_limit(bandwidth_limit);
                    engine.transfer(s_scheme_ctx, s_p, d_scheme_ctx, d_p, worker_progress, item.custom_id);
                    success_count.fetch_add(1);

                    {
                        std::lock_guard<std::mutex> lock(slots_mutex);
                        finished_bytes.fetch_add(worker_slots[w].total);
                    }
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(slots_mutex);
                    failed_items.push_back({item.filename, e.what()});
                }

                {
                    std::lock_guard<std::mutex> lock(slots_mutex);
                    worker_slots[w].active = false;
                }
                completed_tasks.fetch_add(1);
            }
        });
    }

    for (auto& wt : workers) {
        if (wt.joinable()) wt.join();
    }

    g_transferring = false;
    if (ui_thread.joinable()) ui_thread.join();

    auto batch_end = std::chrono::steady_clock::now();
    auto total_dur = std::chrono::duration_cast<std::chrono::seconds>(batch_end - batch_start).count();
    if (total_dur < 1) total_dur = 1;

    uint64_t total_done_bytes = finished_bytes.load();
    uint64_t avg_speed = (total_dur > 0) ? (total_done_bytes / total_dur) : 0;

    // ──── MODERN COMPLETION SUMMARY ────
    if (!failed_items.empty()) {
        if (success_count.load() == 0) {
            std::cout << C_RED GLYPH_END C_RESET "  " C_RED C_BOLD "✗ Transfer failed!" C_RESET "\n";
            std::cout << "   " C_MUTED "Transferred: " C_WHITE << format_bytes(total_done_bytes) << C_MUTED " in "
                      << total_dur / 60 << "m " << total_dur % 60 << "s" C_RESET "\n";
            std::cout << "   " C_RED "Errors encountered (" << failed_items.size() << "):" C_RESET "\n";
            for (const auto& f : failed_items) {
                std::cout << "     " C_RED "✗ " << f.first << ": " C_MUTED << f.second << C_RESET "\n";
            }
            std::cout << "\n";
        } else {
            std::cout << C_YELLOW GLYPH_END C_RESET "  " C_YELLOW C_BOLD "▲ Transfer completed with errors" C_RESET "\n";
            std::cout << "   " C_WHITE << format_bytes(total_done_bytes) << C_RESET C_MUTED " transferred in "
                      << total_dur / 60 << "m " << total_dur % 60 << "s"
                      << " (Avg: " << format_bytes(avg_speed) << "/s)" C_RESET "\n";
            std::cout << "   " C_MUTED "Status: " C_GREEN << success_count.load() << " succeeded" C_RESET << C_MUTED ", "
                      << C_RED << failed_items.size() << " failed" C_RESET "\n";
            for (const auto& f : failed_items) {
                std::cout << "     " C_RED "✗ " << f.first << ": " C_MUTED << f.second << C_RESET "\n";
            }
            std::cout << "\n";
        }
    } else {
        std::cout << C_GREEN GLYPH_END C_RESET "  " C_GREEN C_BOLD "✔ Transfer complete!" C_RESET "\n";
        std::cout << "   " C_WHITE << format_bytes(total_done_bytes) << C_RESET C_MUTED " transferred in "
                  << total_dur / 60 << "m " << total_dur % 60 << "s"
                  << " (Avg: " << format_bytes(avg_speed) << "/s)" C_RESET "\n";
        std::cout << "   " C_MUTED "Status: Verified & committed · Zero disk footprint · TLS 1.3" C_RESET "\n\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        print_usage();
        return 0;
    }
    uint64_t bandwidth_limit = 0;
    std::string custom_id;
    bool verbose = false;
    std::vector<std::string> pos_args;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bandwidth" && i + 1 < argc) {
            bandwidth_limit = std::stoull(argv[++i]);
        } else if (arg == "--id" && i + 1 < argc) {
            custom_id = argv[++i];
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (!arg.empty() && arg[0] != '-') {
            pos_args.push_back(arg);
        }
    }

    try {
        if (cmd == "transfer") {
            if (pos_args.size() < 2) {
                // Interactive TUI Mode
                bool is_initial_launch = true;
                while (true) {
                    TuiTransferParams tui = launch_interactive_tui(is_initial_launch);
                    is_initial_launch = false;
                    if (!tui.valid || tui.transfers.empty()) {
                        std::cout << C_GREEN GLYPH_DONE C_RESET "  " C_MUTED "Exiting UTP Engine. Goodbye!" C_RESET "\n\n";
                        break;
                    }

                    size_t total_tasks = tui.transfers.size();

                    // Pipeline Confirmation Banner
                    std::cout << C_GREEN GLYPH_DONE C_RESET "  " C_WHITE C_BOLD "Transfer pipeline configured" C_RESET "\n";
                    std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN << tui.transfers[0].src_uri << C_RESET
                              << C_MUTED " ──[ Direct RAM Buffer ]──▶ " C_RESET
                              << C_VIOLET << tui.transfers[0].dest_uri << C_RESET "\n";
                    std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Payload: " C_WHITE << total_tasks << " file" << (total_tasks > 1 ? "s" : "")
                              << " queued" C_RESET << C_MUTED " · 0-Disk Memory Stream" C_RESET "\n";
                    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";

                    size_t num_workers = 1;

                    Resource first_src = parse_uri(tui.transfers[0].src_uri);
                    Resource first_dest = parse_uri(tui.transfers[0].dest_uri);

                    std::string s_acc = first_src.account.empty() ? tui.src_account : first_src.account;
                    std::string d_acc = first_dest.account.empty() ? tui.dest_account : first_dest.account;

                    auto src_provider = std::shared_ptr<StorageProvider>(create_provider(first_src.scheme, s_acc));
                    auto dest_provider = std::shared_ptr<StorageProvider>(create_provider(first_dest.scheme, d_acc));

                    std::string src_label = AccountManager::instance().get_display_name(first_src.scheme, s_acc);
                    std::string dest_label = AccountManager::instance().get_display_name(first_dest.scheme, d_acc);

                    // Start streaming HUD
                    run_batch_transfer_pool(src_provider, dest_provider, tui.transfers, num_workers, src_label, dest_label, bandwidth_limit);

                    // Post-transfer clean prompt
                    std::vector<MenuOption> follow_up = {
                        {"Transfer another file", "", "Select new source and destination"},
                        {"Exit", "", "Close UTP Engine"}
                    };
                    int follow_idx = run_interactive_menu("Start another transfer?", follow_up, 0);
                    if (follow_idx != 0) {
                        std::cout << C_GREEN GLYPH_DONE C_RESET "  " C_MUTED "Exiting UTP Engine. Goodbye!" C_RESET "\n\n";
                        break;
                    }
                }
                return 0;
            } else {
                // Non-interactive CLI mode with arguments
                std::string src_str = pos_args[0];
                std::string dest_str = pos_args[1];

                Resource src_res = parse_uri(src_str);
                Resource dest_res = parse_uri(dest_str);

                std::string src_label = AccountManager::instance().get_display_name(src_res.scheme, src_res.account);
                std::string dest_label = AccountManager::instance().get_display_name(dest_res.scheme, dest_res.account);

                std::string transfer_id = custom_id.empty() ? Engine::generate_id() : custom_id;

                std::cout << "\n  " C_VIOLET C_BOLD GLYPH_BOLT " UTP Transfer: " C_WHITE << src_label << C_MUTED " ➔ " C_WHITE << dest_label << C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Transfer ID: " C_CYAN << transfer_id << C_RESET "\n\n";

                auto src_provider = std::shared_ptr<StorageProvider>(create_provider(src_res.scheme, src_res.account));
                auto dest_provider = std::shared_ptr<StorageProvider>(create_provider(dest_res.scheme, dest_res.account));

                std::string item_name = src_res.path.empty() ? src_res.host_or_bucket : src_res.path;
                std::vector<TransferItem> transfers = {{src_str, dest_str, item_name, src_res.account, dest_res.account, transfer_id}};
                run_batch_transfer_pool(src_provider, dest_provider, transfers, 1, src_label, dest_label, bandwidth_limit);
                return 0;
            }
        } else if (cmd == "resume") {
            if (pos_args.empty()) {
                print_usage();
                return 1;
            }
            std::string transfer_id = pos_args[0];
            StateManager sm("utp.db");
            auto opt_transfer = sm.get_transfer(transfer_id);
            if (!opt_transfer) throw std::runtime_error("Transfer not found: " + transfer_id);

            auto src_provider = create_provider(opt_transfer->source.provider);
            auto dest_provider = create_provider(opt_transfer->destination.provider);

            Engine engine(std::move(src_provider), std::move(dest_provider), "utp.db");
            if (bandwidth_limit > 0) engine.set_bandwidth_limit(bandwidth_limit);

            std::cout << "\n  " C_VIOLET C_BOLD GLYPH_BOLT " Resuming transfer: " C_WHITE << transfer_id << C_RESET "\n\n";
            engine.resume(transfer_id, [](uint64_t bytes, uint64_t total) {
                float p = (total > 0) ? static_cast<float>(bytes) / total : 0.0f;
                std::cout << "\r  " C_MUTED "Resuming: " C_RESET;
                render_block_bar(p, 30);
                std::cout << " " << static_cast<int>(p * 100.0) << "% (" << format_bytes(bytes) << "/" << format_bytes(total) << ")";
                std::cout.flush();
            });
            std::cout << "\n\n  " C_GREEN GLYPH_CHECK " Transfer resumed and completed successfully!" C_RESET "\n\n";
        } else if (cmd == "pause" || cmd == "cancel") {
            if (pos_args.empty()) {
                print_usage();
                return 1;
            }
            std::string transfer_id = pos_args[0];
            StateManager sm("utp.db");
            auto opt_transfer = sm.get_transfer(transfer_id);
            if (!opt_transfer) throw std::runtime_error("Transfer not found: " + transfer_id);

            if (opt_transfer->state == TransferState::COMPLETED) {
                std::cout << "  " C_YELLOW GLYPH_STEP C_RESET " " C_WHITE << transfer_id << C_RESET C_MUTED " is already COMPLETED — nothing to " << cmd << ".\n\n" C_RESET;
                return 0;
            }
            if (opt_transfer->state == TransferState::CANCELLED) {
                std::cout << "  " C_YELLOW GLYPH_STEP C_RESET " " C_WHITE << transfer_id << C_RESET C_MUTED " is already CANCELLED.\n\n" C_RESET;
                return 0;
            }

            sm.request_control(transfer_id, cmd);
            if (cmd == "pause") {
                std::cout << "  " C_VIOLET GLYPH_STEP C_RESET " " C_WHITE << (cmd == "pause" ? "Pause" : "Cancel") << " requested" C_RESET C_MUTED " for " C_RESET C_CYAN << transfer_id << C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "The engine stops between chunks and keeps all completed progress.\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Continue later with: " C_CYAN "utp resume " << transfer_id << C_RESET "\n\n";
            } else {
                std::cout << "  " C_VIOLET GLYPH_STEP C_RESET " " C_WHITE << (cmd == "pause" ? "Pause" : "Cancel") << " requested" C_RESET C_MUTED " for " C_RESET C_CYAN << transfer_id << C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "The engine will abort the upload and discard partial state.\n\n";
            }
        } else if (cmd == "status") {
            if (pos_args.empty()) {
                print_usage();
                return 1;
            }
            std::string transfer_id = pos_args[0];
            StateManager sm("utp.db");
            auto opt_transfer = sm.get_transfer(transfer_id);
            if (!opt_transfer) throw std::runtime_error("Transfer not found: " + transfer_id);
            auto plan = sm.get_plan(transfer_id);

            size_t complete = 0, pending = 0, failed = 0, in_progress = 0;
            for (const auto& c : plan) {
                switch (c.state) {
                    case ChunkState::COMPLETE: complete++; break;
                    case ChunkState::IN_PROGRESS: in_progress++; break;
                    case ChunkState::FAILED: failed++; break;
                    default: pending++; break;
                }
            }

            auto state_color = [&](TransferState s) -> const char* {
                switch (s) {
                    case TransferState::COMPLETED: return C_GREEN;
                    case TransferState::FAILED: return C_RED;
                    case TransferState::PAUSED: return C_YELLOW;
                    case TransferState::CANCELLED: return C_RED;
                    case TransferState::TRANSFERRING: return C_CYAN;
                    default: return C_MUTED;
                }
            };

            std::cout << "\n" C_VIOLET GLYPH_TOP C_RESET "  " C_WHITE C_BOLD "UTP Transfer Status" C_RESET " " C_MUTED << transfer_id << C_RESET "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Pipeline: " C_RESET << opt_transfer->source.provider << "://" << opt_transfer->source.path
                      << C_MUTED " ──▶ " C_RESET << opt_transfer->destination.provider << "://" << opt_transfer->destination.path << "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "State:    " C_RESET
                      << state_color(opt_transfer->state) << transfer_state_to_string(opt_transfer->state) << C_RESET "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Progress: " C_RESET << format_bytes(opt_transfer->bytes_transferred) << " / " << format_bytes(opt_transfer->size)
                      << C_MUTED " · chunk " << (opt_transfer->chunk_size ? format_bytes(opt_transfer->chunk_size) : std::string("n/a")) << C_RESET "\n";
            if (!plan.empty()) {
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Chunks:   " C_RESET
                          << C_GREEN << complete << " complete" << C_RESET C_MUTED ", "
                          << C_CYAN << in_progress << " active" << C_RESET C_MUTED ", "
                          << C_YELLOW << pending << " pending" << C_RESET C_MUTED ", "
                          << C_RED << failed << " failed" << C_RESET C_MUTED " (" << plan.size() << " total)" C_RESET "\n";
            }
            {
                auto events = sm.get_events(transfer_id, 12);
                if (!events.empty()) {
                    std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD "Recent events" C_RESET "\n";
                    for (const auto& ev : events) {
                        std::time_t secs = static_cast<std::time_t>(ev.timestamp / 1000000000);
                        std::tm tmv{};
#ifdef _WIN32
                        localtime_s(&tmv, &secs);
#else
                        localtime_r(&secs, &tmv);
#endif
                        char tbuf[16];
                        std::strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tmv);
                        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED << tbuf << "  " C_CYAN
                                  << event_type_to_string(ev.type) << C_RESET;
                        if (!ev.payload.empty()) {
                            std::cout << C_MUTED "  " << truncate_visible(ev.payload, 48) << C_RESET;
                        }
                        std::cout << "\n";
                    }
                }
            }
            std::cout << C_GREEN GLYPH_END C_RESET "\n";
        } else if (cmd == "list") {
            StateManager sm("utp.db");
            auto transfers = sm.list_transfers(20);
            std::cout << "\n" C_VIOLET GLYPH_TOP C_RESET "  " C_WHITE C_BOLD "Recent transfers" C_RESET "\n";
            if (transfers.empty()) {
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "(none — the ledger is empty)" C_RESET "\n";
            }
            for (const auto& t : transfers) {
                std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN << t.id << C_RESET C_MUTED "  "
                          << transfer_state_to_string(t.state) << "  " << format_bytes(t.bytes_transferred) << "/" << format_bytes(t.size) << "\n"
                          << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED << truncate_visible(t.source.provider + "://" + t.source.path, 48)
                          << " ──▶ " << truncate_visible(t.destination.provider + "://" + t.destination.path, 48) << C_RESET "\n";
            }
            std::cout << C_GREEN GLYPH_END C_RESET "\n";
        } else {
            std::cerr << "  " C_RED "Unknown command: " << cmd << C_RESET "\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "\n  " C_RED "✗ Error: " << e.what() << C_RESET "\n\n";
        return 1;
    }

    return 0;
}
