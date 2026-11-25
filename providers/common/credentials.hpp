#pragma once

// Clean credential resolver: reads OAuth client credentials strictly from
// environment variables, gitignored local `.utp_credentials.json` / `.env`,
// or user profile `~/.utp/credentials.json`. Zero client secrets are stored
// or obfuscated in source code.

#include "utp/security_utils.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace utp {

inline std::string trim_credential_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

inline std::vector<std::filesystem::path> credential_search_dirs() {
    namespace fs = std::filesystem;
    std::vector<fs::path> dirs;
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) dirs.push_back(cwd);

#ifdef _WIN32
    char exe_path[MAX_PATH] = {0};
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        fs::path exe_dir = fs::path(exe_path).parent_path();
        if (!exe_dir.empty() && exe_dir != cwd) dirs.push_back(exe_dir);
    }
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home && std::strlen(home) > 0) {
        dirs.push_back(fs::path(home) / ".utp");
    }
    return dirs;
}

inline std::string read_credential_file(const std::string& key) {
    namespace fs = std::filesystem;
    for (const auto& dir : credential_search_dirs()) {
        for (const char* fname : {".utp_credentials.json", "credentials.json"}) {
            fs::path cred_path = dir / fname;
            std::ifstream cred_file(cred_path);
            if (cred_file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(cred_file)), std::istreambuf_iterator<char>());
                std::string val = utp::security::extract_json_string(content, key);
                if (!val.empty()) return trim_credential_str(val);
            }
        }
        fs::path env_path = dir / ".env";
        std::ifstream env_file(env_path);
        if (env_file.is_open()) {
            std::string line;
            while (std::getline(env_file, line)) {
                line = trim_credential_str(line);
                if (line.empty() || line[0] == '#') continue;
                if (line.rfind(key + "=", 0) == 0) {
                    return trim_credential_str(line.substr(key.length() + 1));
                }
            }
        }
    }
    return "";
}

inline std::string resolve_gdrive_client_id() {
    const char* env_id = std::getenv("GDRIVE_CLIENT_ID");
    if (!env_id) env_id = std::getenv("UTP_GDRIVE_CLIENT_ID");
    if (!env_id) env_id = std::getenv("GOOGLE_CLIENT_ID");
    if (env_id && std::strlen(env_id) > 0) return trim_credential_str(env_id);

    std::string from_file = read_credential_file("GDRIVE_CLIENT_ID");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("gdrive_client_id");
    if (!from_file.empty()) return from_file;

    // Public OAuth Client ID (not a secret; override via GDRIVE_CLIENT_ID if desired)
    return "617211477359-8a1s4v0aamr860vrs2k9bn9h6lou534u.apps.googleusercontent.com";
}

inline std::string resolve_gdrive_client_secret(const std::string& /*client_id*/ = "") {
    const char* env_val = std::getenv("GDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("UTP_GDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("UTP_GOOGLE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("GOOGLE_CLIENT_SECRET");
    if (env_val && std::strlen(env_val) > 0) return trim_credential_str(env_val);

    std::string from_file = read_credential_file("GDRIVE_CLIENT_SECRET");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("gdrive_client_secret");
    if (!from_file.empty()) return from_file;

    return "";
}

inline std::string resolve_onedrive_client_id() {
    const char* env_id = std::getenv("ONEDRIVE_CLIENT_ID");
    if (!env_id) env_id = std::getenv("UTP_ONEDRIVE_CLIENT_ID");
    if (!env_id) env_id = std::getenv("MICROSOFT_CLIENT_ID");
    if (env_id && std::strlen(env_id) > 0) return trim_credential_str(env_id);

    std::string from_file = read_credential_file("ONEDRIVE_CLIENT_ID");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("onedrive_client_id");
    if (!from_file.empty()) return from_file;

    // Public Application (Client) ID (not a secret; override via ONEDRIVE_CLIENT_ID if desired)
    return "5091c9e9-674d-466c-849a-e1bf7d6badee";
}

inline std::string resolve_onedrive_client_secret(const std::string& /*client_id*/ = "") {
    const char* env_val = std::getenv("ONEDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("UTP_ONEDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("MICROSOFT_CLIENT_SECRET");
    if (env_val && std::strlen(env_val) > 0) return trim_credential_str(env_val);

    std::string from_file = read_credential_file("ONEDRIVE_CLIENT_SECRET");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("onedrive_client_secret");
    if (!from_file.empty()) return from_file;

    return "";
}

} // namespace utp
