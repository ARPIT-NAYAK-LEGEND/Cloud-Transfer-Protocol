#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include "utp/security_utils.hpp"

namespace utp {

struct Account {
    std::string id;          // identifier/alias used in context: e.g. "my_personal", "my_work", "default"
    std::string provider;    // "gdrive", "onedrive", "s3", "file"
    std::string name;        // User-friendly display name, e.g. "Personal Drive", "Work Drive"
    std::string email;       // Email / user identifier
    std::string token_file;  // Token file path
};

class AccountManager {
public:
    static AccountManager& instance() {
        static AccountManager mgr;
        return mgr;
    }

    void reload() {
        accounts_.clear();
        load_accounts();
    }

    std::vector<Account> get_accounts(const std::string& provider) {
        std::vector<Account> res;
        for (const auto& a : accounts_) {
            if (a.provider == provider) res.push_back(a);
        }
        return res;
    }

    size_t count_accounts(const std::string& provider) {
        return get_accounts(provider).size();
    }

    std::string get_display_name(const std::string& provider, const std::string& account_id) {
        for (const auto& a : accounts_) {
            if (a.provider == provider && a.id == account_id) {
                if (!a.email.empty() && a.email.find('@') != std::string::npos) {
                    return a.name + " (" + a.email + ")";
                }
                return a.name;
            }
        }
        if (provider == "gdrive") {
            return account_id.empty() ? "Google Drive" : ("Google Drive (" + account_id + ")");
        }
        if (provider == "onedrive") {
            return (account_id.empty() || account_id == "default") ? "OneDrive" : ("OneDrive (" + account_id + ")");
        }
        if (provider == "dropbox") {
            if (account_id == "default" || account_id.empty()) return "Dropbox";
            return "Dropbox (" + account_id + ")";
        }
        if (provider == "icloud") {
            if (account_id == "local" || account_id == "bridge") return "Local iCloud Drive";
            if (account_id == "default" || account_id.empty()) return "iCloud Drive";
            return "iCloud (" + account_id + ")";
        }
        if (provider == "s3") return "AWS S3";
        if (provider == "r2") return "Cloudflare R2";
        if (provider == "b2") return "Backblaze B2";
        if (provider == "azure") return "Azure Blob";
        if (provider == "webdav" || provider == "nextcloud" || provider == "owncloud") return "WebDAV / Nextcloud";
        if (provider == "http" || provider == "https") return "Web Direct Stream (" + provider + ")";
        if (provider == "file" || provider == "local") return "Local Disk";
        return provider;
    }

    void add_account(const Account& acc) {
        Account to_add = acc;
        if (to_add.email.empty() || to_add.email.find('@') == std::string::npos) {
            std::ifstream tf(to_add.token_file);
            if (tf.is_open()) {
                std::string t_content((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
                std::string em = extract_value(t_content, "email");
                if (em.empty()) em = extract_value(t_content, "user_name");
                if (!em.empty() && em.find('@') != std::string::npos) to_add.email = em;
            }
        }
        // Replace if already exists with same id & provider
        for (auto& existing : accounts_) {
            if (existing.provider == to_add.provider && existing.id == to_add.id) {
                if (!to_add.email.empty() && to_add.email.find('@') != std::string::npos) {
                    existing.email = to_add.email;
                }
                existing.name = to_add.name;
                existing.token_file = to_add.token_file;
                save_accounts();
                return;
            }
        }
        accounts_.push_back(to_add);
        save_accounts();
    }

    void update_account_email(const std::string& provider, const std::string& account_id, const std::string& email) {
        if (email.empty()) return;
        for (auto& a : accounts_) {
            if (a.provider == provider && a.id == account_id) {
                a.email = email;
                save_accounts();
                return;
            }
        }
    }

    std::string get_token_file(const std::string& provider, const std::string& account_id) {
        for (const auto& a : accounts_) {
            if (a.provider == provider && a.id == account_id) {
                return a.token_file;
            }
        }
        if (provider == "gdrive") {
            return ".utp_gdrive_" + (account_id.empty() ? "default" : account_id) + "_token.json";
        }
        if (provider == "onedrive") {
            return (account_id.empty() || account_id == "default") ? ".utp_onedrive_token.json" : (".utp_onedrive_" + account_id + "_token.json");
        }
        if (provider == "dropbox") {
            return (account_id.empty() || account_id == "default") ? ".utp_dropbox_token.json" : (".utp_dropbox_" + account_id + "_token.json");
        }
        if (provider == "icloud") {
            return (account_id.empty() || account_id == "default") ? ".utp_icloud_token.json" : (".utp_icloud_" + account_id + "_token.json");
        }
        return "";
    }

private:
    AccountManager() {
        load_accounts();
    }

    std::vector<Account> accounts_;

    static std::string extract_value(const std::string& json, const std::string& key) {
        return utp::security::extract_json_string(json, key);
    }

    void load_accounts() {
        // 1. Try reading .utp_accounts.json if it exists
        std::ifstream file(".utp_accounts.json");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            parse_accounts_json(content);
        }

        // 2. Discover token files on disk automatically
        discover_local_token_files();

        // 3. Resolve real emails
        for (auto& a : accounts_) {
            if (a.email.empty() || a.email.find('@') == std::string::npos) {
                std::ifstream tf(a.token_file);
                if (tf.is_open()) {
                    std::string t_content((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
                    std::string em = extract_value(t_content, "email");
                    if (em.empty()) em = extract_value(t_content, "user_name");
                    if (!em.empty() && em.find('@') != std::string::npos) {
                        a.email = em;
                    }
                }
            }
        }

        // 4. Persist registry
        save_accounts();
    }

    void parse_accounts_json(const std::string& json) {
        size_t arr_start = json.find("\"accounts\":[");
        if (arr_start == std::string::npos) arr_start = json.find("\"accounts\": [");
        if (arr_start == std::string::npos) return;
        arr_start = json.find('[', arr_start);
        size_t arr_end = json.rfind(']');
        if (arr_end == std::string::npos || arr_end <= arr_start) return;

        size_t curr = arr_start;
        while (curr < arr_end) {
            size_t obj_start = json.find('{', curr);
            if (obj_start == std::string::npos || obj_start >= arr_end) break;
            size_t obj_end = json.find('}', obj_start);
            if (obj_end == std::string::npos || obj_end > arr_end) break;

            std::string item_json = json.substr(obj_start, obj_end - obj_start + 1);
            Account a;
            a.id = extract_value(item_json, "id");
            a.provider = extract_value(item_json, "provider");
            a.name = extract_value(item_json, "name");
            a.email = extract_value(item_json, "email");
            a.token_file = extract_value(item_json, "token_file");

            if (!a.id.empty() && !a.provider.empty()) {
                // Ensure no duplicates
                bool found = false;
                for (auto& existing : accounts_) {
                    if (existing.provider == a.provider && existing.id == a.id) {
                        existing = a;
                        found = true;
                        break;
                    }
                }
                if (!found) accounts_.push_back(a);
            }
            curr = obj_end + 1;
        }
    }

    void discover_local_token_files() {
        namespace fs = std::filesystem;
        try {
            for (const auto& entry : fs::directory_iterator(".")) {
                if (!entry.is_regular_file()) continue;
                std::string fname = entry.path().filename().string();

                // Google Drive: .utp_gdrive_<id>_token.json
                if (fname.rfind(".utp_gdrive_", 0) == 0 && fname.length() > 23 && fname.substr(fname.length() - 11) == "_token.json") {
                    std::string id = fname.substr(12, fname.length() - 12 - 11);
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "gdrive" && a.id == id) { found = true; break; }
                    }
                    if (!found) {
                        std::string label = (id == "personal" || id == "default") ? "Personal Google Drive" : (id == "work") ? "Work Google Drive" : ("Google Drive (" + id + ")");
                        accounts_.push_back({id, "gdrive", label, id, fname});
                    }
                }

                // OneDrive: .utp_onedrive_token.json
                if (fname == ".utp_onedrive_token.json") {
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "onedrive" && a.id == "default") { found = true; break; }
                    }
                    if (!found) {
                        accounts_.push_back({"default", "onedrive", "OneDrive", "", fname});
                    }
                }

                // OneDrive: .utp_onedrive_<id>_token.json
                if (fname.rfind(".utp_onedrive_", 0) == 0 && fname.length() > 25 && fname.substr(fname.length() - 11) == "_token.json") {
                    std::string id = fname.substr(14, fname.length() - 14 - 11);
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "onedrive" && a.id == id) { found = true; break; }
                    }
                    if (!found) {
                        std::string label = (id == "work") ? "Work OneDrive" :
                                            (id == "personal") ? "Personal OneDrive" :
                                            ("OneDrive (" + id + ")");
                        accounts_.push_back({id, "onedrive", label, id, fname});
                    }
                }

                // Dropbox: .utp_dropbox_token.json
                if (fname == ".utp_dropbox_token.json") {
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "dropbox" && a.id == "default") { found = true; break; }
                    }
                    if (!found) {
                        accounts_.push_back({"default", "dropbox", "Dropbox", "Personal/Team", fname});
                    }
                }

                // Dropbox: .utp_dropbox_<id>_token.json
                if (fname.rfind(".utp_dropbox_", 0) == 0 && fname.length() > 24 && fname.substr(fname.length() - 11) == "_token.json") {
                    std::string id = fname.substr(13, fname.length() - 13 - 11);
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "dropbox" && a.id == id) { found = true; break; }
                    }
                    if (!found) {
                        std::string label = (id == "work") ? "Work Dropbox" :
                                            (id == "personal") ? "Personal Dropbox" :
                                            ("Dropbox (" + id + ")");
                        accounts_.push_back({id, "dropbox", label, id, fname});
                    }
                }

                // iCloud: .utp_icloud_token.json
                if (fname == ".utp_icloud_token.json") {
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "icloud" && a.id == "default") { found = true; break; }
                    }
                    if (!found) {
                        accounts_.push_back({"default", "icloud", "iCloud Drive", "Apple ID Web", fname});
                    }
                }

                // iCloud: .utp_icloud_<id>_token.json
                if (fname.rfind(".utp_icloud_", 0) == 0 && fname.length() > 23 && fname.substr(fname.length() - 11) == "_token.json") {
                    std::string id = fname.substr(12, fname.length() - 12 - 11);
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "icloud" && a.id == id) { found = true; break; }
                    }
                    if (!found) {
                        std::string label = (id == "work") ? "Work iCloud" :
                                            (id == "personal") ? "Personal iCloud" :
                                            ("iCloud (" + id + ")");
                        accounts_.push_back({id, "icloud", label, id, fname});
                    }
                }
            }

            // Check if local iCloud Drive directory is available on this system
#ifdef _WIN32
            const char* user_prof = std::getenv("USERPROFILE");
            if (user_prof) {
                fs::path p1 = fs::path(user_prof) / "iCloudDrive";
                fs::path p2 = fs::path(user_prof) / "Apple" / "iCloudDrive";
                if ((fs::exists(p1) && fs::is_directory(p1)) || (fs::exists(p2) && fs::is_directory(p2))) {
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "icloud" && a.id == "local") { found = true; break; }
                    }
                    if (!found) {
                        accounts_.push_back({"local", "icloud", "Local iCloud Drive", "Local Sync Bridge", ""});
                    }
                }
            }
#else
            const char* home = std::getenv("HOME");
            if (home) {
                fs::path p = fs::path(home) / "Library" / "Mobile Documents" / "com~apple~CloudDocs";
                if (fs::exists(p) && fs::is_directory(p)) {
                    bool found = false;
                    for (const auto& a : accounts_) {
                        if (a.provider == "icloud" && a.id == "local") { found = true; break; }
                    }
                    if (!found) {
                        accounts_.push_back({"local", "icloud", "Local iCloud Drive", "Local Sync Bridge", ""});
                    }
                }
            }
#endif
        } catch (...) {}
    }

    void save_accounts() {
        std::ostringstream oss;
        oss << "{\n  \"accounts\": [\n";
        for (size_t i = 0; i < accounts_.size(); ++i) {
            const auto& a = accounts_[i];
            oss << "    {\n";
            oss << "      \"id\": \"" << utp::security::escape_json(a.id) << "\",\n";
            oss << "      \"provider\": \"" << utp::security::escape_json(a.provider) << "\",\n";
            oss << "      \"name\": \"" << utp::security::escape_json(a.name) << "\",\n";
            oss << "      \"email\": \"" << utp::security::escape_json(a.email) << "\",\n";
            oss << "      \"token_file\": \"" << utp::security::escape_json(a.token_file) << "\"\n";
            oss << "    }" << (i + 1 < accounts_.size() ? "," : "") << "\n";
        }
        oss << "  ]\n}\n";
        utp::security::write_secret_file(".utp_accounts.json", oss.str());
    }
};

} // namespace utp
