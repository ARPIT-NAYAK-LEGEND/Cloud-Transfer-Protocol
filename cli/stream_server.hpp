#pragma once

#include <string>
#include <cctype>

namespace utp {

// Returns standard MIME type based on file extension
inline std::string get_media_mime_type(const std::string& filename) {
    std::string ext = "";
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) {
        ext = filename.substr(dot);
        for (char& c : ext) c = static_cast<char>(std::tolower(c));
    }
    if (ext == ".mp4" || ext == ".m4v") return "video/mp4";
    if (ext == ".mkv") return "video/x-matroska";
    if (ext == ".webm") return "video/webm";
    if (ext == ".avi") return "video/x-msvideo";
    if (ext == ".mov") return "video/quicktime";
    if (ext == ".wmv") return "video/x-ms-wmv";
    if (ext == ".flv") return "video/x-flv";
    if (ext == ".ts") return "video/mp2t";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".flac") return "audio/flac";
    if (ext == ".wav") return "audio/wav";
    if (ext == ".m4a" || ext == ".aac") return "audio/mp4";
    if (ext == ".ogg" || ext == ".oga") return "audio/ogg";
    if (ext == ".pdf") return "application/pdf";
    return "application/octet-stream";
}

// Runs the zero-disk on-demand cloud media HTTP Range streaming server
int run_cloud_stream_server(const std::string& cloud_uri, int port = 8080, bool auto_open = false);

} // namespace utp
