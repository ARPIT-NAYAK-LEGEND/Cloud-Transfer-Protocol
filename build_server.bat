@echo off
set PATH=C:\msys64\usr\bin;C:\msys64\ucrt64\bin;%PATH%
echo Compiling Cloud Transfer Protocol API Server...
g++ -std=c++20 -D_WIN32_WINNT=0x0A00 -I core/include -I . -I server server/main.cpp core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp providers/azure/azure_provider.cpp providers/common/http_client.cpp providers/dropbox/dropbox_provider.cpp providers/google_drive/gdrive_provider.cpp providers/icloud/icloud_provider.cpp providers/local/local_provider.cpp providers/onedrive/onedrive_provider.cpp providers/s3/s3_provider.cpp providers/webdav/webdav_provider.cpp -o utp-server.exe -lcurl -lssl -lcrypto -lws2_32 -lsqlite3
if %errorlevel% neq 0 (
    echo Compilation Failed! Please ensure you have MSYS2 with gcc, libcurl, sqlite3, and openssl installed.
) else (
    echo Compilation Successful! Created utp-server.exe
    echo You can start the daemon using: .\utp-server.exe
)
