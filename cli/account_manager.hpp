#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <iostream>

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
            if (account_id == "my_personal") return "Personal Google Drive (arpitpupu@gmail.com)";
            if (account_id == "my_work") return "Work Google Drive (arpit.work@gmail.com)";
            return account_id.empty() ? "Google Drive" : ("Google Drive (" + account_id + ")");
        }
        if (provider == "onedrive") {
            if (account_id == "default" || account_id.empty()) return "Primary OneDrive (zyphe4@outlook.com)";
            return "OneDrive (" + account_id + ")";
        }
        if (provider == "s3") return "AWS S3";
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
        return "";
    }

private:
    AccountManager() {
        load_accounts();
    }

    std::vector<Account> accounts_;

    static std::string extract_value(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) pos++;
        if (pos >= json.length()) return "";
        if (json[pos] == '"') {
            pos++;
            size_t end = pos;
            while (end < json.length()) {
                if (json[end] == '\\' && end + 1 < json.length()) end += 2;
                else if (json[end] == '"') break;
                else end++;
            }
            return json.substr(pos, end - pos);
        }
        return "";
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
                if (a.email.empty() || a.email.find('@') == std::string::npos) {
                    if (a.provider == "gdrive") {
                        if (a.id == "my_personal" || a.id == "personal") a.email = "arpitpupu@gmail.com";
                        else if (a.id == "my_work" || a.id == "work") a.email = "arpit.work@gmail.com";
                    } else if (a.provider == "onedrive") {
                        if (a.id == "default" || a.id == "personal") a.email = "zyphe4@outlook.com";
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
                        std::string label = (id == "my_personal") ? "Personal Drive" :
                                            (id == "my_work") ? "Work Drive" :
                                            (id == "default") ? "Primary Drive" :
                                            ("Google Drive (" + id + ")");
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
                        accounts_.push_back({"default", "onedrive", "Primary OneDrive", "Personal/Work", fname});
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
            }
        } catch (...) {}
    }

    void save_accounts() {
        std::ofstream file(".utp_accounts.json");
        if (!file.is_open()) return;

        file << "{\n  \"accounts\": [\n";
        for (size_t i = 0; i < accounts_.size(); ++i) {
            const auto& a = accounts_[i];
            file << "    {\n";
            file << "      \"id\": \"" << a.id << "\",\n";
            file << "      \"provider\": \"" << a.provider << "\",\n";
            file << "      \"name\": \"" << a.name << "\",\n";
            file << "      \"email\": \"" << a.email << "\",\n";
            file << "      \"token_file\": \"" << a.token_file << "\"\n";
            file << "    }" << (i + 1 < accounts_.size() ? "," : "") << "\n";
        }
        file << "  ]\n}\n";
    }
};

} // namespace utp
