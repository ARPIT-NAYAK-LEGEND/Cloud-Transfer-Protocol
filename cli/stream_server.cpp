#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#elif _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include "cli/stream_server.hpp"
#include "cli/account_manager.hpp"
#include "cli/tui.hpp"
#include "server/httplib.h"
#include "providers/interface/storage_provider.hpp"
#include "utp/uri_parser.hpp"
#include "utp/security_utils.hpp"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <atomic>
#include <csignal>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

#define C_RESET       "\033[0m"
#define C_BOLD        "\033[1m"
#define C_MUTED       "\033[38;5;242m"
#define C_WHITE       "\033[38;5;255m"
#define C_EMERALD     "\033[38;5;48m"
#define C_CYAN        "\033[38;5;45m"
#define C_VIOLET      "\033[38;5;141m"
#define C_AMBER       "\033[38;5;214m"
#define C_ROSE        "\033[38;5;204m"
#define C_SPINE       "\033[38;5;238m"

#define GLYPH_STEP    "◇"
#define GLYPH_DONE    "◆"
#define GLYPH_SPINE   "│"
#define GLYPH_CHECK   "✔"

namespace utp {

static std::atomic<httplib::Server*> g_active_stream_server{nullptr};

bool stop_active_stream_server() {
    auto* s = g_active_stream_server.load();
    if (s && s->is_running()) {
        s->stop();
        return true;
    }
    return false;
}

static void stream_signal_handler(int) {
    stop_active_stream_server();
}

static std::string format_file_size(uint64_t bytes) {
    if (bytes == 0) return "0 B";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double d_size = static_cast<double>(bytes);
    while (d_size >= 1024.0 && unit_idx < 4) {
        d_size /= 1024.0;
        unit_idx++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << d_size << " " << units[unit_idx];
    return ss.str();
}

int run_cloud_stream_server(const std::string& cloud_uri, int port, bool auto_open) {
    std::cout << "\n"
              << C_VIOLET "▲" C_RESET " " C_WHITE C_BOLD "UTP" C_RESET " "
              << C_MUTED "· Zero-Disk Cloud Stream Engine" C_RESET " "
              << C_CYAN "v2.6.2" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "  "
              << C_MUTED "Direct RAM Range Pipe · Zero Local Disk · TLS 1.3" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "\n";

    Resource res;
    try {
        res = parse_uri(cloud_uri);
    } catch (const std::exception& e) {
        std::cerr << C_ROSE "✗ Invalid cloud URI: " << e.what() << C_RESET "\n\n";
        return 1;
    }

    std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " Connecting to "
              << AccountManager::instance().get_display_name(res.scheme, res.account) << "...\n";

    std::shared_ptr<StorageProvider> provider;
    try {
        provider = std::shared_ptr<StorageProvider>(create_provider(res.scheme, res.account));
    } catch (const std::exception& e) {
        std::cerr << C_ROSE "✗ Connection failed: " << e.what() << C_RESET "\n\n";
        return 1;
    }

    std::string remote_path = res.path.empty() ? res.host_or_bucket : res.path;
    if (res.scheme == "file" || res.scheme == "local") {
        remote_path = res.host_or_bucket + (res.path.empty() ? "" : ("/" + res.path));
    }

    std::string filename = "";
    uint64_t file_size = 0;
    try {
        FileInfo fi = provider->stat(remote_path);
        filename = fi.name.empty() ? remote_path : fi.name;
        file_size = fi.size;
    } catch (const std::exception& e) {
        std::cerr << C_AMBER "▲ Warning: Could not stat remote file (" << e.what() << "). Using path name." C_RESET "\n";
        size_t slash = remote_path.find_last_of("/\\");
        filename = (slash != std::string::npos) ? remote_path.substr(slash + 1) : remote_path;
    }

    if (filename.empty()) filename = "video.mp4";
    std::string safe_filename = utp::security::sanitize_header_filename(filename);
    std::string mime = get_media_mime_type(safe_filename);

    ReadStream rstream;
    try {
        rstream = provider->openRead(remote_path);
    } catch (const std::exception& e) {
        std::cerr << C_ROSE "✗ Failed to open remote stream: " << e.what() << C_RESET "\n\n";
        return 1;
    }

    httplib::Server svr;
    g_active_stream_server.store(&svr);
    std::signal(SIGINT, stream_signal_handler);
    std::signal(SIGTERM, stream_signal_handler);

    svr.Get("/favicon.ico", [](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
    });

    std::atomic<uint64_t> total_bytes_streamed{0};

    auto stream_handler = [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Access-Control-Allow-Origin", "http://127.0.0.1");
        res.set_header("Access-Control-Allow-Headers", "Range, Accept, Content-Type");
        res.set_header("Content-Disposition", "inline; filename=\"" + safe_filename + "\"");

        res.set_content_provider(
            file_size,
            mime,
            [provider, rstream, file_size, &total_bytes_streamed](size_t offset, size_t length, httplib::DataSink& sink) -> bool {
                const size_t CHUNK = 512 * 1024; // 512 KB in-memory volatile chunk
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
                        return false; // Socket closed or seek initiated by player
                    }
                    cur_offset += got;
                    remaining -= got;
                    total_bytes_streamed += got;
                }
                return true;
            }
        );
    };

    svr.Get(".*", stream_handler);

    // Find available port
    int bound_port = -1;
    for (int p = port; p < port + 20; ++p) {
        if (svr.bind_to_port("127.0.0.1", p)) {
            bound_port = p;
            break;
        }
    }

    if (bound_port <= 0) {
        std::cerr << C_ROSE "✗ Error: Unable to bind to 127.0.0.1 on ports " << port << "-" << (port + 19) << C_RESET "\n\n";
        return 1;
    }

    std::string stream_url = "http://127.0.0.1:" + std::to_string(bound_port) + "/stream";

    std::cout << C_EMERALD GLYPH_DONE C_RESET "  " C_WHITE C_BOLD "Source Endpoint : " C_RESET
              << AccountManager::instance().get_display_name(res.scheme, res.account) << "\n"
              << C_SPINE GLYPH_SPINE C_RESET "  " C_WHITE C_BOLD "Remote File     : " C_RESET
              << filename << C_MUTED " (" << format_file_size(file_size) << " · " << mime << ")" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "  " C_WHITE C_BOLD "Stream URL      : " C_RESET
              << C_CYAN << stream_url << C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "\n"
              << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD "PLAYBACK INSTRUCTIONS" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "VLC Media Player : " C_RESET "vlc \"" << stream_url << "\"\n"
              << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "MPV Player       : " C_RESET "mpv \"" << stream_url << "\"\n"
              << C_SPINE GLYPH_SPINE C_RESET "    " C_CYAN "Web Browser      : " C_RESET << stream_url << "\n"
              << C_SPINE GLYPH_SPINE C_RESET "\n"
              << C_EMERALD GLYPH_DONE C_RESET "  " C_EMERALD C_BOLD "Streaming live with Zero Local Disk Footprint." C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Press [q] or [Enter] or [Ctrl+C] to stop streaming and return to menu." C_RESET "\n\n";

    if (auto_open) {
        std::thread([stream_url]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            utp::security::open_url_safely(stream_url);
        }).detach();
    }

    std::thread server_thread([&]() {
        svr.listen_after_bind();
    });

    while (svr.is_running()) {
#ifdef _WIN32
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q' || ch == 13 || ch == 27 || ch == 3) { // q, Q, Enter, Esc, Ctrl+C
                svr.stop();
                break;
            }
        }
#else
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 'q' || c == 'Q' || c == '\n' || c == 27) {
                svr.stop();
                break;
            }
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (svr.is_running()) {
        svr.stop();
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }

    std::cout << "\n" C_EMERALD GLYPH_DONE C_RESET "  " C_MUTED "Streaming server stopped. Total transferred: "
              << format_file_size(total_bytes_streamed.load()) << " directly in RAM." C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_EMERALD "Returning to menu..." C_RESET "\n\n";

    g_active_stream_server.store(nullptr);
    return 0;
}

} // namespace utp
