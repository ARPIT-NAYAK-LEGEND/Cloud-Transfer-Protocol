#pragma once

#include "providers/interface/storage_provider.hpp"
#include <string>
#include <memory>
#include <vector>

namespace utp {

struct TransferItem {
    std::string src_uri;
    std::string dest_uri;
    std::string filename;
    std::string src_account{""};
    std::string dest_account{""};
    std::string custom_id{""};
    uint64_t file_size{0};
};

struct TuiTransferParams {
    std::string src_uri;
    std::string dest_uri;
    std::string src_account{""};
    std::string dest_account{""};
    std::string src_display{""};
    std::string dest_display{""};
    std::string target_folder{""};
    std::vector<TransferItem> transfers;
    bool valid{false};
};

struct MenuOption {
    std::string title;
    std::string scheme;
    std::string desc;
};

// Interactive keyboard-driven menu (arrow keys or 1-9 shortcuts, Enter to confirm)
int run_interactive_menu(const std::string& question, const std::vector<MenuOption>& options, int default_idx = 0);

// Print the modern UTP visual header banner
void print_modern_header(bool is_initial = true);

// Launch the interactive Text User Interface (TUI) for transferring files
TuiTransferParams launch_interactive_tui(bool is_initial_launch = true);

// Launch the interactive cloud file picker to stream media directly via HTTP Range server
int launch_interactive_stream_picker(int port = 8080, bool auto_open = true);

std::unique_ptr<StorageProvider> create_provider(const std::string& scheme, const std::string& context = "");

} // namespace utp
