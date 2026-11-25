#include "tui.hpp"
#include "account_manager.hpp"
#include "stream_server.hpp"
#include "providers/http/http_provider.hpp"
#include "utp/security_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <cctype>
#include <ctime>
#include <numeric>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

namespace utp {

// ═══════════════════════════════════════════════════════════════════════════════
//  DESIGN TOKENS & ANSI COLOR PALETTE (CLAUDE CODE / VERCEL / STRIPE)
// ═══════════════════════════════════════════════════════════════════════════════

#define C_RESET     "\033[0m"
#define C_BOLD      "\033[1m"
#define C_DIM       "\033[2m"
#define C_VIOLET    "\033[38;5;141m" // Signature Violet (#a78bfa)
#define C_CYAN      "\033[38;5;51m"  // Electric Cyan (#38bdf8)
#define C_BLUE      "\033[38;5;75m"  // Soft Blue (#60a5fa)
#define C_EMERALD   "\033[38;5;49m"  // Vibrant Emerald (#34d399)
#define C_AMBER     "\033[38;5;215m" // Warm Amber (#fbbf24)
#define C_ROSE      "\033[38;5;203m" // Coral Red (#f87171)
#define C_WHITE     "\033[1;37m"     // Crisp White
#define C_DIM_W     "\033[0;37m"     // Regular Soft White
#define C_MUTED     "\033[38;5;242m" // Slate Gray (#64748b)
#define C_SPINE     "\033[38;5;239m" // Vertical Spine Line (#475569)
#define C_BORDER    "\033[38;5;238m" // Card Border Box (#334155)

// Glyphs
#define GLYPH_TOP    "┌"
#define GLYPH_SPINE  "│"
#define GLYPH_STEP   "◇"
#define GLYPH_DONE   "◆"
#define GLYPH_END    "└"
#define GLYPH_STREAM "▲"
#define GLYPH_ON     "●"
#define GLYPH_OFF    "○"
#define GLYPH_CHECK  "✔"
#define GLYPH_PROMPT "❯"

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

static std::string trim(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '"' || s.front() == '\'')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n' || s.back() == '"' || s.back() == '\'')) s.pop_back();
    return s;
}

static std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string pad_right(const std::string& str, size_t width) {
    if (str.length() >= width) return str;
    return str + std::string(width - str.length(), ' ');
}

static std::string truncate_str(const std::string& str, size_t max_len) {
    if (str.length() <= max_len) return str;
    if (max_len <= 3) return str.substr(0, max_len);
    return str.substr(0, max_len - 3) + "...";
}

static std::string get_file_icon(const std::string& name, bool is_dir) {
    if (is_dir) return "📁";
    std::string lower = to_lower(name);
    if (lower.ends_with(".jpg") || lower.ends_with(".jpeg") || lower.ends_with(".png") ||
        lower.ends_with(".gif") || lower.ends_with(".heic") || lower.ends_with(".webp")) return "🖼 ";
    if (lower.ends_with(".mp4") || lower.ends_with(".mov") || lower.ends_with(".mkv") || lower.ends_with(".avi")) return "🎬";
    if (lower.ends_with(".mp3") || lower.ends_with(".wav") || lower.ends_with(".m4a") || lower.ends_with(".flac")) return "🎵";
    if (lower.ends_with(".pdf") || lower.ends_with(".doc") || lower.ends_with(".docx") || lower.ends_with(".txt") || lower.ends_with(".md")) return "📄";
    if (lower.ends_with(".zip") || lower.ends_with(".rar") || lower.ends_with(".7z") || lower.ends_with(".tar") || lower.ends_with(".gz")) return "📦";
    if (lower.ends_with(".xls") || lower.ends_with(".xlsx") || lower.ends_with(".csv") || lower.ends_with(".xlsb")) return "📊";
    return "📄";
}

static std::string get_file_type_label(const std::string& name, bool is_dir) {
    if (is_dir) return "Directory";
    std::string lower = to_lower(name);
    if (lower.ends_with(".mp4")) return "MP4 Video";
    if (lower.ends_with(".mov")) return "QuickTime Video";
    if (lower.ends_with(".mkv")) return "Matroska Video";
    if (lower.ends_with(".avi")) return "AVI Video";
    if (lower.ends_with(".jpg") || lower.ends_with(".jpeg")) return "JPEG Image";
    if (lower.ends_with(".png")) return "PNG Image";
    if (lower.ends_with(".pdf")) return "PDF Document";
    if (lower.ends_with(".zip") || lower.ends_with(".7z") || lower.ends_with(".tar") || lower.ends_with(".gz")) return "Archive";
    if (lower.ends_with(".mp3") || lower.ends_with(".flac") || lower.ends_with(".wav")) return "Audio";
    if (lower.ends_with(".doc") || lower.ends_with(".docx")) return "Word Doc";
    if (lower.ends_with(".txt") || lower.ends_with(".md")) return "Text Document";
    return "File";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  INTERACTIVE MENU (TABULAR ALIGNMENT, ZERO GHOSTING, FAST SHORTCUTS)
// ═══════════════════════════════════════════════════════════════════════════════

int run_interactive_menu(const std::string& question, const std::vector<MenuOption>& options, int default_idx) {
    std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD << question << C_RESET "\n";
    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";

    int selected = default_idx;
    int total = static_cast<int>(options.size());
    if (selected < 0 || selected >= total) selected = 0;

    auto print_choices = [&](bool move_up) {
        if (move_up) {
            std::cout << "\033[" << (total + 2) << "A";
        }
        for (int i = 0; i < total; ++i) {
            std::cout << "\033[2K" C_SPINE GLYPH_SPINE C_RESET "  ";

            // Determine column values cleanly
            std::string title = options[i].title;
            std::string sub = options[i].scheme;
            std::string badge = options[i].desc;

            // Handle internal sentinel values cleanly
            if (sub == "add_new") {
                sub = "(Browser OAuth)";
                if (badge.empty()) badge = "Connect new account";
            }

            std::string col1 = pad_right(truncate_str(title, 28), 28);
            std::string col2 = pad_right(truncate_str(sub, 26), 26);
            std::string col3 = truncate_str(badge, 28);

            if (i == selected) {
                std::cout << C_CYAN GLYPH_PROMPT " " C_VIOLET GLYPH_ON " " C_WHITE C_BOLD << col1 << C_RESET "  "
                          << C_CYAN << col2 << C_RESET "  "
                          << C_AMBER << col3 << C_RESET "\n";
            } else {
                std::cout << "  " C_MUTED GLYPH_OFF " " C_DIM_W << col1 << C_RESET "  "
                          << C_MUTED << col2 << C_RESET "  "
                          << C_MUTED << col3 << C_RESET "\n";
            }
        }
        std::cout << "\033[2K" C_SPINE GLYPH_SPINE C_RESET "\n";
        std::cout << "\033[2K" C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "↑/↓ navigate · 1-" << total
                  << " select · Enter confirm · Esc cancel" C_RESET "\n";
        std::cout.flush();
    };

    print_choices(false);

    while (true) {
#ifdef _WIN32
        int ch = _getch();
        if (ch == 3 || ch == 'q' || ch == 'Q' || ch == 27) { // Ctrl+C, q, Esc
            return -1;
        }
        if (ch == 13) { // Enter
            break;
        }
        if (ch >= '1' && ch < '1' + total && ch <= '9') {
            selected = ch - '1';
            break;
        }
        if (ch == 0 || ch == 224) {
            int code = _getch();
            if (code == 72) { // Up
                selected = (selected - 1 + total) % total;
                print_choices(true);
            } else if (code == 80) { // Down
                selected = (selected + 1) % total;
                print_choices(true);
            }
        }
#else
        std::string line;
        if (!std::getline(std::cin, line)) return -1;
        line = trim(line);
        if (line == "q" || line == "exit") return -1;
        if (!line.empty() && std::isdigit(line[0])) {
            int val = line[0] - '1';
            if (val >= 0 && val < total) { selected = val; break; }
        }
        break;
#endif
    }

    // Collapse cleanly into confirmed step
    std::cout << "\033[" << (total + 3) << "A";
    std::cout << "\033[2K" C_EMERALD GLYPH_DONE C_RESET "  " C_MUTED << question << ": "
              << C_WHITE C_BOLD << options[selected].title << C_RESET;

    std::string tag = options[selected].scheme;
    if (!tag.empty() && tag != "add_new" && tag.find("://") == std::string::npos && tag.find('@') != std::string::npos) {
        std::cout << " " C_CYAN "(" << tag << ")" C_RESET;
    }
    std::cout << "\n";
    std::cout << "\033[2K" C_SPINE GLYPH_SPINE C_RESET "\n";

    // Clear remaining choice lines smoothly
    for (int i = 0; i < total + 1; ++i) {
        std::cout << "\033[2K\n";
    }
    std::cout << "\033[" << (total + 1) << "A";
    std::cout.flush();

    return selected;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HEADER
// ═══════════════════════════════════════════════════════════════════════════════

static void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[3J\033[H\033[2J";
    std::cout.flush();
#endif
}

void print_modern_header(bool is_initial) {
    if (is_initial) clear_screen();
    std::cout << "\n"
              << C_VIOLET "▲" C_RESET " " C_WHITE C_BOLD "UTP" C_RESET " "
              << C_MUTED "· Universal Cloud Transfer Protocol" C_RESET " "
              << C_CYAN "v2.6.2-pro" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "  "
              << C_MUTED "Dynamic Memory Ring Buffer · Zero-Disk Pipeline · TLS 1.3" C_RESET "\n"
              << C_SPINE GLYPH_SPINE C_RESET "\n";
    std::cout.flush();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  PAGINATED FILE EXPLORER
// ═══════════════════════════════════════════════════════════════════════════════

struct FolderBreadcrumb {
    std::string id;
    std::string name;
};

struct ExplorerResult {
    bool cancelled{false};
    std::vector<FileInfo> selected_files;
    std::string chosen_folder{""};
    std::string chosen_folder_name{""};
    bool is_folder_chosen{false};
};

static std::vector<size_t> parse_selection(std::string input, size_t count, const std::vector<FileInfo>& items) {
    std::vector<size_t> indices;
    for (char& c : input) {
        if (c == ',' || c == '[' || c == ']') c = ' ';
    }
    std::stringstream ss(input);
    std::string token;
    while (ss >> token) {
        std::string low = to_lower(token);
        if (low == "all" || low == "*") {
            for (size_t i = 0; i < count; ++i) {
                if (!items[i].is_directory) indices.push_back(i);
            }
            return indices;
        }

        size_t dash = token.find('-');
        if (dash == std::string::npos) dash = token.find("..");
        if (dash != std::string::npos && dash > 0 && dash + 1 < token.length()) {
            std::string s1 = token.substr(0, dash);
            size_t step = (token[dash] == '.') ? 2 : 1;
            std::string s2 = token.substr(dash + step);
            if (!s1.empty() && !s2.empty() && std::all_of(s1.begin(), s1.end(), ::isdigit) && std::all_of(s2.begin(), s2.end(), ::isdigit)) {
                size_t start = std::stoul(s1);
                size_t end = std::stoul(s2);
                if (start > end) std::swap(start, end);
                for (size_t k = start; k <= end; ++k) {
                    if (k >= 1 && k <= count) {
                        indices.push_back(k - 1);
                    }
                }
                continue;
            }
        }

        if (!token.empty() && std::all_of(token.begin(), token.end(), ::isdigit)) {
            size_t k = std::stoul(token);
            if (k >= 1 && k <= count) {
                indices.push_back(k - 1);
            }
        }
    }

    std::vector<size_t> unique;
    for (size_t idx : indices) {
        if (std::find(unique.begin(), unique.end(), idx) == unique.end()) {
            unique.push_back(idx);
        }
    }
    return unique;
}

static ExplorerResult run_drive_explorer(const std::string& scheme, const std::string& account_id, const std::string& provider_name, bool is_source) {
    ExplorerResult res;

    std::cout << C_CYAN GLYPH_STEP C_RESET "  " C_MUTED "Connecting to " << provider_name << "..." C_RESET "\n";
    std::cout.flush();

    std::unique_ptr<StorageProvider> provider;
    try {
        provider = create_provider(scheme, account_id);
    } catch (const std::exception& e) {
        std::cout << "\033[1A\033[2K";
        std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "Connection failed: " << e.what() << C_RESET "\n\n";
        res.cancelled = true;
        return res;
    }

    // Overwrite connection line
    std::cout << "\033[1A\033[2K";

    std::vector<FolderBreadcrumb> gdrive_stack = {{"root", ""}};
    std::string current_path = "";
    size_t current_page = 0;
    const size_t PAGE_SIZE = 12;

    while (true) {
        std::string query_target;
        std::string display_path;
        if (scheme == "gdrive") {
            query_target = gdrive_stack.back().id;
            if (gdrive_stack.size() == 1) {
                display_path = "/";
            } else {
                display_path = "";
                for (size_t k = 1; k < gdrive_stack.size(); ++k) {
                    display_path += "/" + gdrive_stack[k].name;
                }
            }
        } else {
            query_target = current_path;
            display_path = current_path.empty() ? "/" : ("/" + current_path);
        }

        std::vector<FileInfo> items;
        try {
            items = provider->list(query_target);
        } catch (const std::exception& e) {
            std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "Error fetching directory: " << e.what() << C_RESET "\n";
        }

        size_t total_items = items.size();
        size_t total_pages = (total_items + PAGE_SIZE - 1) / PAGE_SIZE;
        if (total_pages == 0) total_pages = 1;
        if (current_page >= total_pages) current_page = total_pages - 1;

        size_t start_idx = current_page * PAGE_SIZE;
        size_t end_idx = std::min(start_idx + PAGE_SIZE, total_items);

        uint64_t total_file_size = 0;
        for (const auto& it : items) {
            if (!it.is_directory) total_file_size += it.size;
        }

        // Header
        std::cout << C_VIOLET GLYPH_STEP C_RESET "  " C_WHITE C_BOLD
                  << (is_source ? "Select file(s) to transfer" : "Select destination directory") << C_RESET "\n";
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Location: " << C_CYAN << provider_name << C_MUTED " ── " << C_WHITE << display_path
                  << C_MUTED " (" << total_items << " items · " << format_bytes(total_file_size) << ")" C_RESET "\n";
        std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";

        // Table Header
        std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED "#    Name                                      Size        Type" C_RESET "\n";
        std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_BORDER "───────────────────────────────────────────────────────────────────" C_RESET "\n";

        // Rows
        if (items.empty()) {
            std::cout << C_SPINE GLYPH_SPINE C_RESET "    " C_MUTED "(Empty directory)" C_RESET "\n";
        } else {
            for (size_t i = start_idx; i < end_idx; ++i) {
                const auto& it = items[i];
                std::string num_str = (i + 1 < 10 ? "0" : "") + std::to_string(i + 1);
                std::string icon = get_file_icon(it.name, it.is_directory);

                std::string disp_name = it.name.empty() ? it.path : it.name;
                if (it.is_directory) disp_name += "/";
                disp_name = truncate_str(disp_name, 38);

                std::string sz_str = it.is_directory ? "--" : format_bytes(it.size);
                std::string type_label = get_file_type_label(it.name, it.is_directory);
                const char* n_color = it.is_directory ? C_AMBER : C_WHITE;

                if (it.name == "Personal Vault") {
                    sz_str = "[2FA Locked]";
                    type_label = "Restricted";
                    n_color = C_MUTED;
                }

                std::cout << C_SPINE GLYPH_SPINE C_RESET "    "
                          << C_CYAN << "[" << num_str << "]" C_RESET "  "
                          << icon << " "
                          << n_color << std::left << std::setw(38) << disp_name << C_RESET "  "
                          << C_MUTED << std::right << std::setw(10) << sz_str << C_RESET "  "
                          << C_MUTED << std::left << std::setw(12) << type_label << C_RESET "\n";
            }
        }

        std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Shortcuts: [01-99] Select · [..] Parent · [.] Pick folder · [q] Cancel" C_RESET "\n";
        if (total_pages > 1) {
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Pagination: Page " << (current_page + 1) << " of " << total_pages
                      << " · [n] Next · [p] Previous" C_RESET "\n";
        }
        std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";

        // Prompt
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_PROMPT " " C_WHITE << (is_source ? "Select file #" : "Select destination #") << ": " C_RESET;
        std::string cmd;
        if (!std::getline(std::cin, cmd)) {
            res.cancelled = true;
            return res;
        }
        cmd = trim(cmd);
        if (cmd.empty()) continue;
        std::string low = to_lower(cmd);

        if (low == "q" || low == "exit" || low == "quit") {
            res.cancelled = true;
            return res;
        }

        // Pagination
        if (low == "n" || low == "next") {
            if (current_page + 1 < total_pages) current_page++;
            else std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Already on last page." C_RESET "\n";
            continue;
        }
        if (low == "p" || low == "prev" || low == "previous") {
            if (current_page > 0) current_page--;
            else std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Already on first page." C_RESET "\n";
            continue;
        }

        // Parent directory
        if (low == "cd .." || low == "..") {
            if (scheme == "gdrive") {
                if (gdrive_stack.size() > 1) {
                    gdrive_stack.pop_back();
                    current_page = 0;
                }
            } else {
                size_t slash = current_path.find_last_of('/');
                if (slash != std::string::npos) current_path = current_path.substr(0, slash);
                else current_path = "";
                current_page = 0;
            }
            continue;
        }

        // Root directory
        if (low == "cd /" || low == "/" || low == "cd ~") {
            if (scheme == "gdrive") gdrive_stack = {{"root", ""}};
            else current_path = "";
            current_page = 0;
            continue;
        }

        // cd <folder> or cd <#>
        if (low.rfind("cd ", 0) == 0) {
            std::string target = trim(cmd.substr(3));
            if (!target.empty() && std::all_of(target.begin(), target.end(), ::isdigit)) {
                size_t idx = std::stoul(target);
                if (idx >= 1 && idx <= items.size()) {
                    const auto& it = items[idx - 1];
                    if (it.is_directory) {
                        if (scheme == "gdrive") gdrive_stack.push_back({it.path, it.name});
                        else current_path = it.path;
                        current_page = 0;
                        continue;
                    }
                }
            }
            for (const auto& it : items) {
                if (it.is_directory && to_lower(it.name) == to_lower(target)) {
                    if (scheme == "gdrive") gdrive_stack.push_back({it.path, it.name});
                    else current_path = it.path;
                    current_page = 0;
                    break;
                }
            }
            continue;
        }

        // Select current folder (.)
        if (low == "." || low == "select" || low == "here") {
            if (is_source) {
                FileInfo cur_dir;
                if (scheme == "gdrive") {
                    cur_dir.path = gdrive_stack.back().id;
                    cur_dir.name = (gdrive_stack.size() == 1) ? "root" : gdrive_stack.back().name;
                } else {
                    cur_dir.path = current_path.empty() ? "." : current_path;
                    cur_dir.name = current_path.empty() ? "root" : current_path;
                }
                cur_dir.is_directory = true;
                res.selected_files.push_back(cur_dir);
                std::cout << C_EMERALD GLYPH_DONE C_RESET "  " C_WHITE C_BOLD "Selected folder: " << cur_dir.name << C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                return res;
            } else {
                if (scheme == "gdrive") {
                    res.chosen_folder = (gdrive_stack.size() == 1) ? "root" : gdrive_stack.back().id;
                    res.chosen_folder_name = (gdrive_stack.size() == 1) ? "Root" : gdrive_stack.back().name;
                } else {
                    res.chosen_folder = current_path;
                    res.chosen_folder_name = current_path.empty() ? "Root" : current_path;
                }
                res.is_folder_chosen = true;
                std::cout << C_EMERALD GLYPH_DONE C_RESET "  " C_WHITE C_BOLD "Destination folder: " << res.chosen_folder_name << C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                return res;
            }
        }

        // Selection by number (e.g. 1 or 1-4)
        auto selected_indices = parse_selection(cmd, items.size(), items);
        if (!selected_indices.empty()) {
            if (selected_indices.size() == 1) {
                size_t idx = selected_indices[0];
                const auto& chosen = items[idx];
                if (chosen.is_directory) {
                    if (chosen.name == "Personal Vault") {
                        std::cout << "\n" C_SPINE GLYPH_SPINE C_RESET "  " C_AMBER "▲ Notice: Microsoft OneDrive Personal Vault is 2FA/BitLocker protected.\n"
                                  << C_SPINE GLYPH_SPINE C_RESET "    Microsoft Graph API restricts 3rd-party access. Please select a standard folder or root." C_RESET "\n\n";
                        continue;
                    }
                    if (is_source) {
                        std::vector<MenuOption> dir_choices = {
                            {"Open directory", "", "Browse inside " + chosen.name},
                            {"Transfer entire directory", "", "Queue all items recursively in " + chosen.name}
                        };
                        int dc = run_interactive_menu("Folder Action: " + chosen.name, dir_choices, 0);
                        if (dc == 1) {
                            res.selected_files.push_back(chosen);
                            std::cout << C_EMERALD GLYPH_DONE C_RESET "  Selected folder: " C_WHITE C_BOLD << chosen.name << C_RESET "\n";
                            std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                            return res;
                        } else {
                            if (scheme == "gdrive") gdrive_stack.push_back({chosen.path, chosen.name});
                            else current_path = chosen.path;
                            current_page = 0;
                            continue;
                        }
                    } else {
                        // Destination folder selection
                        std::vector<MenuOption> dir_choices = {
                            {"Select '" + chosen.name + "' as destination", "", "Files will transfer into this folder"},
                            {"Navigate inside directory", "", "Enter and browse deeper"}
                        };
                        int dc = run_interactive_menu("Destination Folder: " + chosen.name, dir_choices, 0);
                        if (dc == 0) {
                            res.chosen_folder = chosen.path;
                            res.chosen_folder_name = chosen.name;
                            res.is_folder_chosen = true;
                            std::cout << C_EMERALD GLYPH_DONE C_RESET "  Destination folder: " C_WHITE C_BOLD << chosen.name << C_RESET "\n";
                            std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                            return res;
                        } else {
                            if (scheme == "gdrive") gdrive_stack.push_back({chosen.path, chosen.name});
                            else current_path = chosen.path;
                            current_page = 0;
                            continue;
                        }
                    }
                } else {
                    // Single file selected
                    if (is_source) {
                        std::vector<MenuOption> file_choices = {
                            {"Transfer to cloud / destination", "", "Transfer " + chosen.name + " to another cloud or local disk"},
                            {"Stream & Play directly (Zero-Disk)", "", "Instant playback in VLC or browser without downloading to disk"}
                        };
                        int fc = run_interactive_menu("File Action: " + chosen.name, file_choices, 0);
                        if (fc == 1) {
                            std::string s_prefix = scheme;
                            if (!account_id.empty() && account_id != "default") s_prefix += ":" + account_id;
                            std::string cloud_uri = s_prefix + "://" + chosen.path;
                            run_cloud_stream_server(cloud_uri, 8080, true);
                            res.cancelled = true;
                            return res;
                        }
                    }
                    res.selected_files.push_back(chosen);
                    std::cout << C_EMERALD GLYPH_DONE C_RESET "  Selected: " C_WHITE C_BOLD << chosen.name << C_RESET
                              << C_MUTED " (" << format_bytes(chosen.size) << ")" C_RESET "\n";
                    std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                    return res;
                }
            } else {
                // Multi-file selection
                for (size_t idx : selected_indices) {
                    res.selected_files.push_back(items[idx]);
                }
                std::cout << C_EMERALD GLYPH_DONE C_RESET "  Selected: " C_WHITE C_BOLD
                          << res.selected_files.size() << " files" C_RESET "\n";
                std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
                return res;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN INTERACTIVE TUI FLOW
// ═══════════════════════════════════════════════════════════════════════════════

struct DiscoveredTransferFile {
    std::string src_path;
    std::string rel_path;
    std::string filename;
    uint64_t size{0};
};

static void scan_folder_recursive(StorageProvider* provider,
                                  const std::string& folder_target,
                                  const std::string& rel_prefix,
                                  std::vector<DiscoveredTransferFile>& out_items) {
    try {
        auto items = provider->list(folder_target);
        for (const auto& it : items) {
            if (it.is_directory) {
                std::string sub_prefix = rel_prefix.empty() ? (it.name + "/") : (rel_prefix + it.name + "/");
                scan_folder_recursive(provider, it.path, sub_prefix, out_items);
            } else {
                std::string rel_name = rel_prefix.empty() ? it.name : (rel_prefix + it.name);
                out_items.push_back({it.path, rel_name, it.name, it.size});
            }
        }
    } catch (const std::exception& e) {
        std::cerr << C_SPINE GLYPH_SPINE C_RESET "  " C_ROSE "[Warning] Could not list folder " << folder_target << ": " << e.what() << C_RESET "\n";
    }
}

struct SelectedEndpoint {
    std::string scheme;
    std::string account;
    std::string display_name;
    int provider_index{0};
    bool cancelled{false};
};

static SelectedEndpoint select_storage_endpoint(const std::string& step_title, int default_provider_idx, bool is_source = false) {
    SelectedEndpoint ep;

    AccountManager::instance().reload();
    size_t gdrive_cnt = AccountManager::instance().count_accounts("gdrive");
    size_t onedrive_cnt = AccountManager::instance().count_accounts("onedrive");
    size_t dropbox_cnt = AccountManager::instance().count_accounts("dropbox");
    size_t icloud_cnt = AccountManager::instance().count_accounts("icloud");

    std::string gdrive_desc = (gdrive_cnt == 0) ? "Ready to connect" :
                              (std::to_string(gdrive_cnt) + " account" + (gdrive_cnt > 1 ? "s" : "") + " connected");
    std::string onedrive_desc = (onedrive_cnt == 0) ? "Ready to connect" :
                                (std::to_string(onedrive_cnt) + " account" + (onedrive_cnt > 1 ? "s" : "") + " connected");
    std::string dropbox_desc = (dropbox_cnt == 0) ? "Personal & Team Cloud" :
                               (std::to_string(dropbox_cnt) + " account" + (dropbox_cnt > 1 ? "s" : "") + " connected");
    std::string icloud_desc = (icloud_cnt == 0) ? "Apple iCloud Drive" :
                              (std::to_string(icloud_cnt) + " account" + (icloud_cnt > 1 ? "s" : "") + " connected");

    std::vector<MenuOption> provider_options = {
        {"Google Drive", "gdrive://", gdrive_desc},
        {"OneDrive", "onedrive://", onedrive_desc}
    };
    if (is_source) {
        provider_options.push_back({"Direct Web / URL Stream", "https://", "Stream web file/movie directly to cloud"});
    }
    provider_options.push_back({"Dropbox", "dropbox://", dropbox_desc});
    provider_options.push_back({"Apple iCloud Drive", "icloud://", icloud_desc});
    provider_options.push_back({"Amazon S3", "s3://", "AWS S3 Buckets & Storage"});
    provider_options.push_back({"Cloudflare R2", "r2://", "Zero-Egress Object Storage"});
    provider_options.push_back({"Backblaze B2", "b2://", "Cost-Effective S3 Storage"});
    provider_options.push_back({"WebDAV / Nextcloud", "webdav://", "Nextcloud, ownCloud & NAS"});
    provider_options.push_back({"Local Filesystem", "file://", "Local NVMe & Mounted Disks"});
    if (!is_source) {
        provider_options.push_back({"Stream & Play (Zero-Disk)", "stream://", "Instant on-demand playback in VLC or browser"});
    }

    int p_idx = run_interactive_menu(step_title, provider_options, default_provider_idx);
    if (p_idx < 0) {
        ep.cancelled = true;
        return ep;
    }
    ep.provider_index = p_idx;

    std::string tag = provider_options[p_idx].scheme;
    if (tag == "gdrive://") ep.scheme = "gdrive";
    else if (tag == "onedrive://") ep.scheme = "onedrive";
    else if (tag == "https://") ep.scheme = "https";
    else if (tag == "dropbox://") ep.scheme = "dropbox";
    else if (tag == "icloud://") ep.scheme = "icloud";
    else if (tag == "s3://") ep.scheme = "s3";
    else if (tag == "r2://") ep.scheme = "r2";
    else if (tag == "b2://") ep.scheme = "b2";
    else if (tag == "webdav://") ep.scheme = "webdav";
    else if (tag == "file://") ep.scheme = "file";
    else if (tag == "stream://") ep.scheme = "stream";

    if (ep.scheme == "stream") {
        ep.account = "";
        ep.display_name = "Live Cloud Stream";
        return ep;
    }

    if (ep.scheme == "https" || ep.scheme == "http") {
        ep.account = "";
        ep.display_name = "Direct Web Stream";
        return ep;
    }

    if (ep.scheme == "file") {
        ep.account = "";
        ep.display_name = "Local Disk";
        return ep;
    }
    if (ep.scheme == "s3") {
        ep.account = "default";
        ep.display_name = "AWS S3";
        return ep;
    }
    if (ep.scheme == "r2") {
        ep.account = "default";
        ep.display_name = "Cloudflare R2";
        return ep;
    }
    if (ep.scheme == "b2") {
        ep.account = "default";
        ep.display_name = "Backblaze B2";
        return ep;
    }
    if (ep.scheme == "webdav") {
        ep.account = "default";
        ep.display_name = "WebDAV / Nextcloud";
        return ep;
    }

    std::string prov_name = (ep.scheme == "gdrive") ? "Google Drive" :
                            (ep.scheme == "onedrive") ? "OneDrive" :
                            (ep.scheme == "dropbox") ? "Dropbox" : "iCloud";
    auto accs = AccountManager::instance().get_accounts(ep.scheme);

    if (accs.empty()) {
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " No " << prov_name << " accounts connected yet.\n";
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  Enter alias for this account (e.g. personal, work) [personal]: ";
        std::string alias;
        std::getline(std::cin, alias);
        alias = trim(alias);
        if (alias.empty()) alias = "personal";

        std::string token_file = AccountManager::instance().get_token_file(ep.scheme, alias);
        if (ep.scheme == "icloud" && alias != "local") {
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  Enter Apple ID (e.g. name@icloud.com): ";
            std::string aid;
            std::getline(std::cin, aid);
            aid = trim(aid);
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  Enter Apple session token (dsWebAuthToken) [press Enter for local bridge]: ";
            std::string tok;
            std::getline(std::cin, tok);
            tok = trim(tok);
            if (!tok.empty()) {
                std::string payload = "{\n  \"apple_id\": \"" + utp::security::escape_json(aid) +
                    "\",\n  \"token\": \"" + utp::security::escape_json(tok) + "\",\n  \"mode\": \"web\"\n}\n";
                utp::security::write_secret_file(token_file, payload);
            }
        }

        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " Authorizing new " << prov_name << " (" << alias << ") via browser...\n";
        std::unique_ptr<StorageProvider> test_p;
        try {
            test_p = create_provider(ep.scheme, alias);
        } catch (const std::exception& e) {
            std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "Authentication failed: " << e.what() << C_RESET "\n\n";
            ep.cancelled = true;
            return ep;
        }

        std::string user_email = test_p ? test_p->get_user_identity() : "";
        std::string alias_label = alias;
        if (!alias_label.empty()) alias_label[0] = std::toupper(alias_label[0]);
        std::string disp_name = alias_label + " " + prov_name;
        if (!user_email.empty() && user_email.find('@') != std::string::npos) {
            disp_name += " (" + user_email + ")";
        }
        AccountManager::instance().add_account({alias, ep.scheme, disp_name, user_email, token_file});
        ep.account = alias;
        ep.display_name = AccountManager::instance().get_display_name(ep.scheme, alias);
        std::cout << C_EMERALD GLYPH_DONE C_RESET "  " C_EMERALD C_BOLD << ep.display_name << " connected successfully!" C_RESET "\n\n";
        return ep;
    }

    // Accounts exist: Show menu with existing accounts + Connect new account option
    std::vector<MenuOption> acc_options;
    for (const auto& a : accs) {
        std::string mail = (!a.email.empty() && a.email.find('@') != std::string::npos) ? a.email : "";
        std::string tag = mail.empty() ? a.id : mail;
        std::string desc = mail.empty() ? "OAuth 2.0 Ready" : ("alias: " + a.id);
        acc_options.push_back({a.name, tag, desc});
    }
    acc_options.push_back({"[+] Connect new " + prov_name + " account...", "add_new", "Log in via browser"});

    int acc_idx = run_interactive_menu("Select " + prov_name + " account", acc_options, 0);
    if (acc_idx < 0) {
        ep.cancelled = true;
        return ep;
    }

    if (acc_idx == static_cast<int>(accs.size())) {
        std::cout << C_SPINE GLYPH_SPINE C_RESET "  Enter alias for new " << prov_name << " account (e.g. work, school): ";
        std::string alias;
        std::getline(std::cin, alias);
        alias = trim(alias);
        if (alias.empty()) alias = "account" + std::to_string(accs.size() + 1);

        for (char& c : alias) {
            if (c == ' ' || c == ':' || c == '+' || c == '/') c = '_';
        }

        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " Authorizing new " << prov_name << " (" << alias << ") via browser...\n";
        std::unique_ptr<StorageProvider> test_p;
        try {
            test_p = create_provider(ep.scheme, alias);
        } catch (const std::exception& e) {
            std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "Authentication failed: " << e.what() << C_RESET "\n\n";
            ep.cancelled = true;
            return ep;
        }

        std::string user_email = test_p ? test_p->get_user_identity() : "";
        std::string token_file = AccountManager::instance().get_token_file(ep.scheme, alias);
        std::string alias_label = alias;
        if (!alias_label.empty()) alias_label[0] = std::toupper(alias_label[0]);
        std::string disp_name = alias_label + " " + prov_name;
        if (!user_email.empty() && user_email.find('@') != std::string::npos) {
            disp_name += " (" + user_email + ")";
        }
        AccountManager::instance().add_account({alias, ep.scheme, disp_name, user_email, token_file});
        ep.account = alias;
        ep.display_name = AccountManager::instance().get_display_name(ep.scheme, alias);
        std::cout << C_EMERALD GLYPH_DONE C_RESET "  " C_EMERALD C_BOLD << ep.display_name << " connected successfully!" C_RESET "\n\n";
        return ep;
    }

    ep.account = accs[acc_idx].id;
    ep.display_name = AccountManager::instance().get_display_name(ep.scheme, ep.account);
    return ep;
}

TuiTransferParams launch_interactive_tui(bool is_initial_launch) {
    TuiTransferParams params;
    params.valid = false;

    print_modern_header(is_initial_launch);

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 1: SELECT SOURCE STORAGE & ACCOUNT
    // ─────────────────────────────────────────────────────────────────────────
    SelectedEndpoint src_ep = select_storage_endpoint("Select source provider", 0, true);
    if (src_ep.cancelled) return params;

    std::string src_scheme = src_ep.scheme;
    std::string src_account = src_ep.account;
    std::string src_name = src_ep.display_name;

    std::vector<FileInfo> selected_source_files;

    if (src_scheme == "http" || src_scheme == "https") {
        std::cout << "\n" C_SPINE GLYPH_SPINE C_RESET "  " C_VIOLET GLYPH_STEP C_RESET " " C_WHITE C_BOLD "Direct Web Stream Ingestion" C_RESET "\n"
                  << C_SPINE GLYPH_SPINE C_RESET "  " C_MUTED "Enter or paste direct download URL (video, ISO, archive)" C_RESET "\n"
                  << C_SPINE GLYPH_SPINE C_RESET "\n"
                  << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN "❯ Direct URL: " C_RESET;
        std::string raw_url;
        if (!std::getline(std::cin, raw_url)) {
            params.valid = false;
            return params;
        }
        raw_url = trim(raw_url);
        if (raw_url.empty()) {
            std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "URL cannot be empty." C_RESET "\n\n";
            return params;
        }
        if (raw_url.rfind("http://", 0) != 0 && raw_url.rfind("https://", 0) != 0) {
            raw_url = "https://" + raw_url;
        }

        std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " Inspecting remote web resource...";
        std::cout.flush();

        std::string item_name = "";
        uint64_t file_size = 0;
        try {
            HttpProvider hp;
            FileInfo info = hp.stat(raw_url);
            item_name = info.name;
            file_size = info.size;
            std::cout << " " C_EMERALD GLYPH_CHECK " (" << item_name << " · " << format_bytes(file_size) << ")" C_RESET "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
        } catch (const std::exception& ex) {
            std::cout << "\n" C_SPINE GLYPH_SPINE C_RESET "  " C_AMBER "▲ Notice: Could not retrieve remote file size/headers (" << ex.what() << ")." C_RESET "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  Proceeding with direct streaming ingestion...\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
            size_t slash = raw_url.find_last_of('/');
            if (slash != std::string::npos && slash + 1 < raw_url.length()) {
                item_name = raw_url.substr(slash + 1);
                size_t q = item_name.find('?');
                if (q != std::string::npos) item_name = item_name.substr(0, q);
            }
            if (item_name.empty()) item_name = "downloaded_file";
        }

        FileInfo fi;
        fi.path = raw_url;
        fi.name = item_name;
        fi.size = file_size;
        fi.is_directory = false;
        selected_source_files.push_back(fi);
    } else {
        auto exp_res = run_drive_explorer(src_scheme, src_account, src_name, true);
        if (exp_res.cancelled || exp_res.selected_files.empty()) return params;
        selected_source_files = exp_res.selected_files;
    }

    // Expand directories recursively
    std::unique_ptr<StorageProvider> src_provider_check;
    try {
        src_provider_check = create_provider(src_scheme, src_account);
    } catch (...) {}

    std::vector<DiscoveredTransferFile> expanded_source_files;
    std::string source_root_folder_name = "";
    bool source_was_folder = false;

    for (const auto& sf : selected_source_files) {
        bool is_dir = sf.is_directory;
        std::string dir_name = sf.name.empty() ? sf.path : sf.name;

        if (is_dir && src_provider_check) {
            source_was_folder = true;
            if (source_root_folder_name.empty()) source_root_folder_name = dir_name;
            std::cout << C_SPINE GLYPH_SPINE C_RESET "  " C_CYAN GLYPH_STEP C_RESET " Scanning directory '" << dir_name << "'...";
            std::cout.flush();
            size_t before_cnt = expanded_source_files.size();
            scan_folder_recursive(src_provider_check.get(), sf.path, dir_name + "/", expanded_source_files);
            size_t found_cnt = expanded_source_files.size() - before_cnt;
            std::cout << " " C_EMERALD GLYPH_CHECK " (" << found_cnt << " files discovered)" C_RESET "\n";
            std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
        } else {
            expanded_source_files.push_back({sf.path, sf.name.empty() ? sf.path : sf.name, sf.name.empty() ? sf.path : sf.name, sf.size});
        }
    }

    if (expanded_source_files.empty()) {
        std::cout << C_ROSE "✗" C_RESET "  " C_ROSE "No files selected." C_RESET "\n\n";
        return params;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // STEP 2: SELECT DESTINATION STORAGE & ACCOUNT
    // ─────────────────────────────────────────────────────────────────────────
    int dest_default = (src_ep.provider_index == 1) ? 0 : 1;
    SelectedEndpoint dest_ep = select_storage_endpoint("Select destination provider", dest_default, false);
    if (dest_ep.cancelled) return params;

    std::string dest_scheme = dest_ep.scheme;
    std::string dest_account = dest_ep.account;
    std::string dest_name = dest_ep.display_name;

    if (dest_scheme == "stream") {
        if (!expanded_source_files.empty()) {
            const auto& sf = expanded_source_files[0];
            std::string s_prefix = src_scheme;
            if (!src_account.empty() && src_account != "default") {
                s_prefix += ":" + src_account;
            }
            std::string cloud_uri = (src_scheme == "http" || src_scheme == "https") ? sf.src_path : (s_prefix + "://" + sf.src_path);
            run_cloud_stream_server(cloud_uri, 8080, true);
        }
        params.valid = false;
        return params;
    }

    // Destination folder picker
    auto dest_exp_res = run_drive_explorer(dest_scheme, dest_account, dest_name, false);
    if (dest_exp_res.cancelled) return params;

    std::string target_folder = dest_exp_res.chosen_folder;
    std::string dest_base = target_folder;

    // Clean slashes
    while (!dest_base.empty() && (dest_base.front() == '/' || dest_base.front() == '\\')) dest_base.erase(0, 1);
    while (!dest_base.empty() && (dest_base.back() == '/' || dest_base.back() == '\\')) dest_base.pop_back();
    if (dest_scheme != "gdrive" && (dest_base == "." || dest_base == "root")) {
        dest_base = "";
    }

    std::string custom_single_filename = "";
    if (expanded_source_files.size() == 1 && !source_was_folder) {
        std::string orig_name = expanded_source_files[0].filename;
        std::cout << C_VIOLET GLYPH_STEP C_RESET "  Save as filename [" C_WHITE << orig_name << C_RESET "]: ";
        std::string fn;
        if (std::getline(std::cin, fn)) {
            fn = trim(fn);
            while (!fn.empty() && fn.front() == '/') fn.erase(0, 1);
            if (!fn.empty()) custom_single_filename = fn;
        }
        std::cout << C_SPINE GLYPH_SPINE C_RESET "\n";
    }

    // Build final transfer list
    for (const auto& sf : expanded_source_files) {
        std::string item_filename = sf.filename;
        if (expanded_source_files.size() == 1 && !custom_single_filename.empty()) {
            item_filename = custom_single_filename;
        }

        std::string s_uri;
        if (src_scheme == "http" || src_scheme == "https") {
            s_uri = sf.src_path;
        } else {
            std::string s_prefix = src_scheme;
            if (!src_account.empty() && src_account != "default") {
                s_prefix += ":" + src_account;
            }
            s_uri = s_prefix + "://" + sf.src_path;
        }

        std::string d_prefix = dest_scheme;
        if (!dest_account.empty() && dest_account != "default") {
            d_prefix += ":" + dest_account;
        }
        std::string d_path = dest_base.empty() ? item_filename : (dest_base + "/" + item_filename);
        std::string d_uri = d_prefix + "://" + d_path;

        params.transfers.push_back({s_uri, d_uri, item_filename, src_account, dest_account, "", sf.size});
    }

    if (!params.transfers.empty()) {
        params.src_uri = params.transfers[0].src_uri;
        params.dest_uri = params.transfers[0].dest_uri;
        params.src_account = src_account;
        params.dest_account = dest_account;
        params.src_display = src_name;
        params.dest_display = dest_name;
        params.target_folder = dest_base.empty() ? "/" : ("/" + dest_base);
        params.valid = true;
    }

    return params;
}

int launch_interactive_stream_picker(int port, bool auto_open) {
    print_modern_header(true);

    SelectedEndpoint src_ep = select_storage_endpoint("Select cloud provider to stream from", 0, false);
    if (src_ep.cancelled) return 0;

    auto exp_res = run_drive_explorer(src_ep.scheme, src_ep.account, src_ep.display_name, true);
    if (exp_res.cancelled || exp_res.selected_files.empty()) return 0;

    const auto& chosen = exp_res.selected_files[0];
    if (chosen.is_directory) {
        std::cout << C_ROSE "✗ Cannot stream a folder. Please select a video or audio file." C_RESET "\n\n";
        return 1;
    }

    std::string s_prefix = src_ep.scheme;
    if (!src_ep.account.empty() && src_ep.account != "default") {
        s_prefix += ":" + src_ep.account;
    }
    std::string cloud_uri = s_prefix + "://" + chosen.path;

    return run_cloud_stream_server(cloud_uri, port, auto_open);
}

} // namespace utp
