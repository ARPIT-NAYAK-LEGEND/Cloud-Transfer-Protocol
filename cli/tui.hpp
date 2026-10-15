#pragma once

#include "providers/interface/storage_provider.hpp"
#include <string>
#include <memory>

namespace utp {

struct TransferItem {
    std::string src_uri;
    std::string dest_uri;
    std::string filename;
    std::string src_account{""};
    std::string dest_account{""};
};

struct TuiTransferParams {
    std::string src_uri;
    std::string dest_uri;
    std::string src_account{""};
    std::string dest_account{""};
    std::vector<TransferItem> transfers;
    bool valid;
};

struct MenuOption {
    std::string title;
    std::string scheme;
    std::string desc;
};

// Interactive keyboard-driven menu (arrow keys or 1-9 shortcuts, Enter to confirm)
int run_interactive_menu(const std::string& question, const std::vector<MenuOption>& options, int default_idx = 0);

// Launch the interactive Text User Interface (TUI) for transferring files
TuiTransferParams launch_interactive_tui(bool is_initial_launch = true);

std::unique_ptr<StorageProvider> create_provider(const std::string& scheme, const std::string& context = "");

} // namespace utp
