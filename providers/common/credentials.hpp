#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace utp {

inline std::string trim_credential_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

inline std::string read_credential_file(const std::string& key) {
    // 1. Check local .utp_credentials.json if present
    std::ifstream cred_file(".utp_credentials.json");
    if (cred_file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(cred_file)), std::istreambuf_iterator<char>());
        std::string search = "\"" + key + "\":";
        size_t pos = content.find(search);
        if (pos != std::string::npos) {
            pos += search.length();
            while (pos < content.length() && (content[pos] == ' ' || content[pos] == '"')) pos++;
            size_t end = pos;
            while (end < content.length() && content[end] != '"' && content[end] != ',' && content[end] != '}' && content[end] != '\r' && content[end] != '\n') end++;
            std::string val = content.substr(pos, end - pos);
            if (!val.empty()) return trim_credential_str(val);
        }
    }
    // 2. Check local .env if present
    std::ifstream env_file(".env");
    if (env_file.is_open()) {
        std::string line;
        while (std::getline(env_file, line)) {
            line = trim_credential_str(line);
            if (line.rfind(key + "=", 0) == 0) {
                return trim_credential_str(line.substr(key.length() + 1));
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

    return "617211477359-8a1s4v0aamr860vrs2k9bn9h6lou534u.apps.googleusercontent.com";
}

inline std::string resolve_gdrive_client_secret(const std::string& client_id = "") {
    const char* env_val = std::getenv("GDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("UTP_GDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("GOOGLE_CLIENT_SECRET");
    if (env_val && std::strlen(env_val) > 0) return trim_credential_str(env_val);

    std::string from_file = read_credential_file("GDRIVE_CLIENT_SECRET");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("gdrive_client_secret");
    if (!from_file.empty()) return from_file;

    // Built-in credential fallback for the default Google Client ID
    std::string cid = client_id.empty() ? resolve_gdrive_client_id() : client_id;
    if (cid == "617211477359-8a1s4v0aamr860vrs2k9bn9h6lou534u.apps.googleusercontent.com") {
        static const uint8_t redacted_gd[] = {
            0x00
            0x00
            0x00
        };
        std::string decoded;
        decoded.resize(sizeof(redacted_gd));
        for (size_t i = 0; i < sizeof(redacted_gd); ++i) {
            decoded[i] = static_cast<char>(redacted_gd[i] ^ 0x42);
        }
        return decoded;
    }

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

    return "5091c9e9-674d-466c-849a-e1bf7d6badee";
}

inline std::string resolve_onedrive_client_secret(const std::string& client_id = "") {
    const char* env_val = std::getenv("ONEDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("UTP_ONEDRIVE_CLIENT_SECRET");
    if (!env_val) env_val = std::getenv("MICROSOFT_CLIENT_SECRET");
    if (env_val && std::strlen(env_val) > 0) return trim_credential_str(env_val);

    std::string from_file = read_credential_file("ONEDRIVE_CLIENT_SECRET");
    if (!from_file.empty()) return from_file;
    from_file = read_credential_file("onedrive_client_secret");
    if (!from_file.empty()) return from_file;

    // Built-in credential fallback for the default OneDrive Client ID
    std::string cid = client_id.empty() ? resolve_onedrive_client_id() : client_id;
    if (cid == "5091c9e9-674d-466c-849a-e1bf7d6badee") {
        static const uint8_t redacted_od[] = {
            0x00
            0x00
            0x00
        };
        std::string decoded;
        decoded.resize(sizeof(redacted_od));
        for (size_t i = 0; i < sizeof(redacted_od); ++i) {
            decoded[i] = static_cast<char>(redacted_od[i] ^ 0x42);
        }
        return decoded;
    }

    return "";
}

} // namespace utp
