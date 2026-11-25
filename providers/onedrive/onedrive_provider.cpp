#include "onedrive_provider.hpp"
#include "providers/common/credentials.hpp"
#include "utp/security_utils.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <chrono>

namespace utp {

using utp::security::url_encode;
using utp::security::escape_json;

static std::string url_encode_path(const std::string& path) {
    std::string encoded;
    std::stringstream ss(path);
    std::string segment;
    bool first = true;
    while (std::getline(ss, segment, '/')) {
        if (!first) encoded += "/";
        first = false;
        encoded += url_encode(segment);
    }
    return encoded;
}

static std::string extract_json_value(const std::string& json, const std::string& key) {
    return utp::security::extract_json_string(json, key);
}

OneDriveProvider::OneDriveProvider(const std::string& client_id, const std::string& client_secret, const std::string& name_context)
    : client_id_(client_id), client_secret_(client_secret), name_context_(name_context) {
    if (client_secret_.empty()) {
        client_secret_ = utp::resolve_onedrive_client_secret(client_id_);
    }
    authenticate();
}

OneDriveProvider::~OneDriveProvider() {}

struct OneDriveAuthCache {
    std::string access_token;
    std::string refresh_token;
    std::string user_name;
    std::chrono::steady_clock::time_point expires_at;
};
static std::map<std::string, OneDriveAuthCache> s_onedrive_cache;
static std::mutex s_onedrive_cache_mutex;

void OneDriveProvider::authenticate() {
    std::string prefix = "\033[1;34m[OneDrive]\033[0m ";
    if (client_secret_.empty()) {
        client_secret_ = utp::resolve_onedrive_client_secret(client_id_);
    }
    std::string token_file;
    if (name_context_.empty() || name_context_ == "default" || name_context_ == "src" || name_context_ == "dest") {
        token_file = ".utp_onedrive_token.json";
    } else {
        token_file = ".utp_onedrive_" + name_context_ + "_token.json";
    }

    // 1. Check in-process static cache first (instantaneous, zero network, zero stdout)
    {
        std::lock_guard<std::mutex> lock(s_onedrive_cache_mutex);
        auto it = s_onedrive_cache.find(token_file);
        if (it != s_onedrive_cache.end()) {
            if (std::chrono::steady_clock::now() < it->second.expires_at) {
                access_token_ = it->second.access_token;
                refresh_token_ = it->second.refresh_token;
                user_email_ = it->second.user_name;
                return;
            }
        }
    }

    std::ifstream infile(token_file);
    if (infile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        access_token_ = extract_json_value(content, "access_token");
        refresh_token_ = extract_json_value(content, "refresh_token");
        user_email_ = extract_json_value(content, "email");
        if (user_email_.empty()) user_email_ = extract_json_value(content, "user_name");

        if (!refresh_token_.empty()) {
            std::string url = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
            std::string body = "client_id=" + url_encode(client_id_) +
                               "&grant_type=refresh_token" +
                               "&refresh_token=" + url_encode(refresh_token_) +
                               "&scope=offline_access%20Files.ReadWrite.All";
            if (!client_secret_.empty()) {
                body += "&client_secret=" + url_encode(client_secret_);
            }
            std::vector<std::string> h = {"Content-Type: application/x-www-form-urlencoded"};
            auto r = http_.post(url, body, h);

            if (r.status_code == 200) {
                access_token_ = extract_json_value(r.body, "access_token");
                std::string new_ref = extract_json_value(r.body, "refresh_token");
                if (!new_ref.empty()) refresh_token_ = new_ref;

                std::vector<std::string> auth_header = {"Authorization: Bearer " + access_token_};
                std::string user_name;
                auto me_resp = http_.get("https://graph.microsoft.com/v1.0/me", auth_header);
                if (me_resp.status_code == 200) {
                    std::string mail = extract_json_value(me_resp.body, "mail");
                    if (mail.empty()) mail = extract_json_value(me_resp.body, "userPrincipalName");
                    if (!mail.empty()) user_name = mail;
                    else user_name = extract_json_value(me_resp.body, "displayName");
                }
                if (user_name.empty()) {
                    auto user_resp = http_.get("https://graph.microsoft.com/v1.0/me/drive", auth_header);
                    if (user_resp.status_code == 200) {
                        user_name = extract_json_value(user_resp.body, "email");
                        if (user_name.empty()) user_name = extract_json_value(user_resp.body, "userPrincipalName");
                        if (user_name.empty()) user_name = extract_json_value(user_resp.body, "displayName");
                    }
                }
                if (user_name.empty()) user_name = "Connected Account";
                user_email_ = user_name;

                std::string new_json = "{\"access_token\":\"" + escape_json(access_token_) +
                                       "\",\"refresh_token\":\"" + escape_json(refresh_token_) +
                                       "\",\"email\":\"" + escape_json(user_name) + "\"}";
                utp::security::write_secret_file(token_file, new_json);

                {
                    std::lock_guard<std::mutex> lock(s_onedrive_cache_mutex);
                    s_onedrive_cache[token_file] = {
                        access_token_,
                        refresh_token_,
                        user_name,
                        std::chrono::steady_clock::now() + std::chrono::minutes(50)
                    };
                }
                return;
            }
        } else if (!access_token_.empty()) {
            std::lock_guard<std::mutex> lock(s_onedrive_cache_mutex);
            s_onedrive_cache[token_file] = {
                access_token_,
                refresh_token_,
                user_email_.empty() ? "Connected Account" : user_email_,
                std::chrono::steady_clock::now() + std::chrono::minutes(50)
            };
            return;
        }
    }

    std::cout << prefix << "Authenticating via OAuth2 (PKCE + Loopback)...\n";

    std::string redirect_uri = "http://localhost:8080";
    std::string oauth_state = utp::security::generate_random_hex(16);
    auto pkce = utp::security::generate_pkce_pair();
    std::string auth_url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?"
                           "client_id=" + url_encode(client_id_) +
                           "&response_type=code"
                           "&redirect_uri=" + url_encode(redirect_uri) +
                           "&scope=offline_access%20Files.ReadWrite.All"
                           "&prompt=select_account"
                           "&state=" + url_encode(oauth_state) +
                           "&code_challenge=" + url_encode(pkce.challenge) +
                           "&code_challenge_method=S256";

    std::cout << prefix << "Opening browser for authentication...\n";
    if (!utp::security::open_url_safely(auth_url)) {
        std::cout << prefix << "Open this URL in your browser:\n  " << auth_url << "\n";
    }

    std::cout << prefix << "Waiting for authorization callback on http://127.0.0.1:8080...\n";
    std::string auth_code = utp::security::capture_oauth_code_loopback(8080, oauth_state, "OneDrive");

    if (auth_code.empty()) {
        throw std::runtime_error("Failed to capture valid OAuth authorization code from loopback callback");
    }

    std::string token_url = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    std::string body = "client_id=" + url_encode(client_id_) +
                       "&scope=offline_access%20Files.ReadWrite.All" +
                       "&code=" + url_encode(auth_code) +
                       "&code_verifier=" + url_encode(pkce.verifier) +
                       "&redirect_uri=" + url_encode(redirect_uri) +
                       "&grant_type=authorization_code";

    if (!client_secret_.empty()) {
        body += "&client_secret=" + url_encode(client_secret_);
    }

    std::vector<std::string> headers = {
        "Content-Type: application/x-www-form-urlencoded"
    };

    auto resp = http_.post(token_url, body, headers);

    if (resp.status_code == 200) {
        access_token_ = extract_json_value(resp.body, "access_token");
        refresh_token_ = extract_json_value(resp.body, "refresh_token");

        std::vector<std::string> auth_header = {"Authorization: Bearer " + access_token_};
        std::string user_name;
        auto me_resp = http_.get("https://graph.microsoft.com/v1.0/me", auth_header);
        if (me_resp.status_code == 200) {
            std::string mail = extract_json_value(me_resp.body, "mail");
            if (mail.empty()) mail = extract_json_value(me_resp.body, "userPrincipalName");
            if (!mail.empty()) user_name = mail;
            else user_name = extract_json_value(me_resp.body, "displayName");
        }
        if (user_name.empty()) {
            auto user_resp = http_.get("https://graph.microsoft.com/v1.0/me/drive", auth_header);
            if (user_resp.status_code == 200) {
                user_name = extract_json_value(user_resp.body, "email");
                if (user_name.empty()) user_name = extract_json_value(user_resp.body, "userPrincipalName");
                if (user_name.empty()) user_name = extract_json_value(user_resp.body, "displayName");
            }
        }
        if (user_name.empty()) user_name = "Connected Account";
        user_email_ = user_name;

        std::cout << prefix << "Authentication successful! Authenticated as: \033[1;32m" << user_name << "\033[0m\n";

        std::string new_json = "{\"access_token\":\"" + escape_json(access_token_) +
                               "\",\"refresh_token\":\"" + escape_json(refresh_token_) +
                               "\",\"email\":\"" + escape_json(user_name) + "\"}";
        utp::security::write_secret_file(token_file, new_json);

        {
            std::lock_guard<std::mutex> lock(s_onedrive_cache_mutex);
            s_onedrive_cache[token_file] = {
                access_token_,
                refresh_token_,
                user_name,
                std::chrono::steady_clock::now() + std::chrono::minutes(50)
            };
        }
    } else {
        std::cerr << prefix << "Authentication failed (HTTP " << resp.status_code << ")\n";
        throw std::runtime_error("OAuth authentication failed");
    }
}

ProviderCapabilities OneDriveProvider::capabilities() {
    return {
        true,  true,  false, true,  true,  true,  true,
        60 * 1024 * 1024, 4
    };
}

std::string OneDriveProvider::token_file_path() const {
    if (name_context_.empty() || name_context_ == "default" || name_context_ == "src" || name_context_ == "dest") {
        return ".utp_onedrive_token.json";
    }
    return ".utp_onedrive_" + name_context_ + "_token.json";
}

// P1.1: forced token refresh for mid-transfer 401s. OneDrive access tokens
// live ~1h; a multi-hour transfer WILL cross the expiry boundary.
void OneDriveProvider::refresh_auth() {
    if (client_secret_.empty()) {
        client_secret_ = utp::resolve_onedrive_client_secret(client_id_);
    }
    if (refresh_token_.empty()) {
        throw TransferError(ErrorCategory::FATAL,
            "OneDrive: no refresh token stored; reconnect this account (delete the .utp_onedrive_*_token.json file and re-run)");
    }
    std::string url = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
    std::string body = "client_id=" + url_encode(client_id_) +
                       "&grant_type=refresh_token" +
                       "&refresh_token=" + url_encode(refresh_token_) +
                       "&scope=offline_access%20Files.ReadWrite.All";
    if (!client_secret_.empty()) {
        body += "&client_secret=" + url_encode(client_secret_);
    }
    auto r = http_.post(url, body, {"Content-Type: application/x-www-form-urlencoded"});
    if (r.status_code != 200) {
        throw TransferError(ErrorCategory::AUTH,
            "OneDrive token refresh failed (" + std::to_string(r.status_code) + ")",
            r.status_code);
    }
    std::string new_access = extract_json_value(r.body, "access_token");
    std::string new_refresh = extract_json_value(r.body, "refresh_token");
    if (new_access.empty()) {
        throw TransferError(ErrorCategory::AUTH,
            "OneDrive token refresh returned no access_token", r.status_code);
    }
    access_token_ = new_access;
    if (!new_refresh.empty()) refresh_token_ = new_refresh;

    std::string token_file = token_file_path();
    {
        std::string updated_json = "{\"access_token\":\"" + escape_json(access_token_) +
                                   "\",\"refresh_token\":\"" + escape_json(refresh_token_) +
                                   "\",\"email\":\"" + escape_json(user_email_) + "\"}";
        utp::security::write_secret_file(token_file, updated_json);
    }
    {
        std::lock_guard<std::mutex> lock(s_onedrive_cache_mutex);
        s_onedrive_cache[token_file] = {access_token_, refresh_token_, user_email_,
                                        std::chrono::steady_clock::now() + std::chrono::minutes(50)};
    }
}

FileInfo OneDriveProvider::stat(Path path) {
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_
    };
    std::string clean_path = path;
    while (!clean_path.empty() && (clean_path.front() == '/' || clean_path.front() == ' ')) clean_path.erase(0, 1);
    while (!clean_path.empty() && (clean_path.back() == '/' || clean_path.back() == ' ')) clean_path.pop_back();

    std::string url;
    if (clean_path.rfind("01", 0) == 0 && clean_path.length() > 20) {
        url = "https://graph.microsoft.com/v1.0/me/drive/items/" + url_encode_path(clean_path);
    } else {
        url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(clean_path);
    }
    auto resp = http_.get(url, headers);

    if (resp.status_code != 200) {
        std::string alt_url = (url.find("/root:/") != std::string::npos) ?
            ("https://graph.microsoft.com/v1.0/me/drive/items/" + url_encode_path(clean_path)) :
            ("https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(clean_path));
        resp = http_.get(alt_url, headers);
        if (resp.status_code != 200) {
            throw TransferError(ErrorCategory::FATAL, "OneDrive stat failed: " + resp.body);
        }
    }

    std::string size_str = extract_json_value(resp.body, "size");
    std::string name_str = extract_json_value(resp.body, "name");
    bool is_dir = (resp.body.find("\"folder\":") != std::string::npos);
    uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);
    std::string sha256_str = extract_json_value(resp.body, "sha256Hash");
    for (char& c : sha256_str) c = std::tolower(static_cast<unsigned char>(c));
    return {path, size, is_dir, name_str.empty() ? path : name_str, 0, sha256_str};
}

// Parse one page of Graph children into `items`.
static void parse_onedrive_children_page(const std::string& body, const std::string& clean_path, std::vector<FileInfo>& items) {
    size_t val_pos = body.find("\"value\":");
    if (val_pos == std::string::npos) return;
    size_t arr_start = body.find('[', val_pos);
    if (arr_start == std::string::npos) return;
    size_t arr_end = body.rfind(']');
    if (arr_end == std::string::npos || arr_end <= arr_start) return;

    size_t curr = arr_start;
    while (curr < arr_end) {
        size_t obj_start = body.find('{', curr);
        if (obj_start == std::string::npos || obj_start >= arr_end) break;

        int depth = 0;
        size_t obj_end = obj_start;
        bool in_str = false;
        for (size_t i = obj_start; i < arr_end; ++i) {
            if (body[i] == '"' && (i == 0 || body[i - 1] != '\\')) {
                in_str = !in_str;
            } else if (!in_str) {
                if (body[i] == '{') depth++;
                else if (body[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        obj_end = i;
                        break;
                    }
                }
            }
        }
        if (obj_end <= obj_start) break;

        std::string item_json = body.substr(obj_start, obj_end - obj_start + 1);
        std::string id = extract_json_value(item_json, "id");
        std::string name = extract_json_value(item_json, "name");
        std::string size_str = extract_json_value(item_json, "size");
        bool is_dir = (item_json.find("\"folder\":") != std::string::npos);
        uint64_t size = size_str.empty() ? 0 : std::stoull(size_str);
        std::string sha256_str = extract_json_value(item_json, "sha256Hash");
        for (char& c : sha256_str) c = std::tolower(static_cast<unsigned char>(c));

        if (!name.empty()) {
            std::string item_path = clean_path.empty() ? name : (clean_path + "/" + name);
            items.push_back({item_path, size, is_dir, name, 0, sha256_str});
        }

        curr = obj_end + 1;
    }
}

std::vector<FileInfo> OneDriveProvider::list(Path path) {
    std::vector<FileInfo> items;
    std::string clean_path = path;
    while (!clean_path.empty() && (clean_path.front() == '/' || clean_path.front() == ' ')) clean_path.erase(0, 1);
    while (!clean_path.empty() && (clean_path.back() == '/' || clean_path.back() == ' ')) clean_path.pop_back();

    std::string url;
    if (clean_path.empty() || clean_path == "." || clean_path == "root") {
        url = "https://graph.microsoft.com/v1.0/me/drive/root/children";
    } else {
        url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(clean_path) + ":/children";
    }

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_
    };

    int page = 0;
    const int max_pages = 100;
    while (!url.empty() && page++ < max_pages) {
        auto resp = http_.get(url, headers);
        if (resp.status_code != 200) {
            std::cerr << "[OneDrive] list failed (" << resp.status_code << "): " << resp.body << "\n";
            return items;
        }

        parse_onedrive_children_page(resp.body, clean_path, items);

        std::string next_link = extract_json_value(resp.body, "@odata.nextLink");
        size_t pos = 0;
        while ((pos = next_link.find("\\/", pos)) != std::string::npos) {
            next_link.replace(pos, 2, "/");
            pos += 1;
        }
        url = next_link;
    }

    return items;
}

void OneDriveProvider::ensure_folder_path(const std::string& folder_path) {
    if (folder_path.empty() || folder_path == "root" || folder_path == ".") return;

    if (folder_path == "Personal Vault" || folder_path.rfind("Personal Vault/", 0) == 0 ||
        folder_path.rfind("Personal_Vault", 0) == 0) {
        throw TransferError(ErrorCategory::FATAL,
            "OneDrive Personal Vault is BitLocker/2FA-locked and restricted from third-party API access by Microsoft. Please select the root directory or a standard folder.");
    }

    std::stringstream ss(folder_path);
    std::string segment;
    std::string current_parent = "";

    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Content-Type: application/json"
    };

    while (std::getline(ss, segment, '/')) {
        while (!segment.empty() && (segment.front() == ' ' || segment.front() == '/')) segment.erase(0, 1);
        while (!segment.empty() && (segment.back() == ' ' || segment.back() == '/')) segment.pop_back();
        if (segment.empty() || segment == "." || segment == "root") continue;

        std::string target_path = current_parent.empty() ? segment : (current_parent + "/" + segment);

        std::string check_url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(target_path);
        auto check_resp = http_.get(check_url, headers);
        if (check_resp.status_code != 200) {
            std::string create_url = current_parent.empty()
                ? "https://graph.microsoft.com/v1.0/me/drive/root/children"
                : ("https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(current_parent) + ":/children");
            std::string body = "{\"name\":\"" + escape_json(segment) + "\",\"folder\":{},\"@microsoft.graph.conflictBehavior\":\"fail\"}";
            auto create_resp = http_.post(create_url, body, headers);
            if (create_resp.status_code != 200 && create_resp.status_code != 201 && create_resp.status_code != 409) {
                std::cerr << "[OneDrive] Note: failed to create intermediate folder '" << target_path
                          << "' (" << create_resp.status_code << "): " << create_resp.body << "\n";
            }
        }
        current_parent = target_path;
    }
}

ReadStream OneDriveProvider::openRead(Path path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "onedrive_read_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, "", 0};
    return {id};
}

WriteSession OneDriveProvider::openWrite(Path path, uint64_t size, const std::string& resume_token) {
    std::string clean_path = path;
    while (!clean_path.empty() && (clean_path.front() == '/' || clean_path.front() == ' ')) clean_path.erase(0, 1);
    while (!clean_path.empty() && (clean_path.back() == '/' || clean_path.back() == ' ')) clean_path.pop_back();
    if (clean_path.empty()) clean_path = "transferred_file";

    if (size == 0) {
        std::vector<std::string> put_headers = {
            "Authorization: Bearer " + access_token_,
            "Content-Type: application/octet-stream"
        };
        std::string put_url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(clean_path) + ":/content";
        auto resp = http_.put(put_url, nullptr, 0, put_headers);
        if (resp.status_code != 200 && resp.status_code != 201) {
            throw TransferError(ErrorCategory::FATAL, "OneDrive create empty file failed (" + std::to_string(resp.status_code) + "): " + resp.body);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::string id = "onedrive_write_" + std::to_string(next_id_++);
        sessions_[id] = {id, path, "", 0};
        return {id};
    }

    std::string upload_url;
    if (!resume_token.empty()) {
        upload_url = resume_token;
    } else {
        std::vector<std::string> headers = {
            "Authorization: Bearer " + access_token_,
            "Content-Type: application/json"
        };

        std::string url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(clean_path) + ":/createUploadSession";
        auto resp = http_.post(url, "{\"item\": {\"@microsoft.graph.conflictBehavior\": \"replace\"}}", headers);

        if (resp.status_code != 200) {
            throw TransferError(ErrorCategory::FATAL, "OneDrive upload session init failed (" + std::to_string(resp.status_code) + "): " + resp.body);
        }

        upload_url = extract_json_value(resp.body, "uploadUrl");
        if (upload_url.empty()) {
            throw TransferError(ErrorCategory::FATAL, "OneDrive missing uploadUrl in response");
        }

        size_t pos = 0;
        while ((pos = upload_url.find("\\/", pos)) != std::string::npos) {
            upload_url.replace(pos, 2, "/");
            pos += 1;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "onedrive_write_" + std::to_string(next_id_++);
    sessions_[id] = {id, path, upload_url, size};
    return {id};
}

std::string OneDriveProvider::write_session_token(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session.id);
    if (it == sessions_.end()) return "";
    return it->second.upload_uri;
}

size_t OneDriveProvider::read(ReadStream session, uint64_t offset, uint8_t* buffer, size_t count) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid OneDrive read session");
        }
        s = sessions_[session.id];
    }

    uint64_t end_byte = offset + count - 1;
    std::vector<std::string> headers = {
        "Authorization: Bearer " + access_token_,
        "Range: bytes=" + std::to_string(offset) + "-" + std::to_string(end_byte)
    };

    std::string url;
    if (s.path.find('/') != std::string::npos || (s.path.length() < 20)) {
        url = "https://graph.microsoft.com/v1.0/me/drive/root:/" + url_encode_path(s.path) + ":/content";
    } else {
        url = "https://graph.microsoft.com/v1.0/me/drive/items/" + url_encode_path(s.path) + "/content";
    }
    auto resp = http_.get(url, headers);

    if (resp.status_code != 200 && resp.status_code != 206) {
        if (resp.status_code == 416) return 0;
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "OneDrive auth error: " + resp.body, resp.status_code);
        }
        if (resp.status_code == 302) {
             auto loc_it = resp.headers.find("Location");
             if (loc_it == resp.headers.end()) loc_it = resp.headers.find("location");
             if (loc_it != resp.headers.end()) {
                 auto redirect_resp = http_.get(loc_it->second, headers);
                 resp = redirect_resp;
             }
        }
        if (resp.status_code != 200 && resp.status_code != 206) {
            throw TransferError(ErrorCategory::RETRYABLE, "OneDrive read failed: " + resp.body, resp.status_code);
        }
    }

    size_t bytes_read = resp.body.size();
    if (bytes_read > count) bytes_read = count;

    std::memcpy(buffer, resp.body.data(), bytes_read);

    return bytes_read;
}

void OneDriveProvider::write(WriteSession session, uint64_t offset, const uint8_t* data, size_t size) {
    if (size == 0) return;
    Session s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(session.id) == sessions_.end()) {
            throw TransferError(ErrorCategory::FATAL, "Invalid OneDrive write session");
        }
        s = sessions_[session.id];
    }
    if (s.upload_uri.empty()) return;

    uint64_t end_byte = offset + size - 1;
    std::vector<std::string> headers = {
        "Content-Length: " + std::to_string(size),
        "Content-Range: bytes " + std::to_string(offset) + "-" + std::to_string(end_byte) + "/" + std::to_string(s.total_size)
    };

    auto resp = http_.put(s.upload_uri, data, size, headers);

    if (resp.status_code != 202 && resp.status_code != 200 && resp.status_code != 201) {
        if (resp.status_code == 401 || resp.status_code == 403) {
            throw TransferError(ErrorCategory::AUTH, "OneDrive auth error", resp.status_code);
        }
        if (resp.status_code == 429) {
            throw TransferError(ErrorCategory::RATE_LIMIT, "OneDrive rate limit", resp.status_code);
        }
        throw TransferError(ErrorCategory::RETRYABLE, "OneDrive write failed: " + resp.body, resp.status_code);
    }
}

void OneDriveProvider::commit(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

void OneDriveProvider::abort(WriteSession session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session.id);
}

} // namespace utp
