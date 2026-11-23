@echo off
set PATH=C:\msys64\usr\bin;C:\msys64\ucrt64\bin;%PATH%
echo Compiling Cloud Transfer Protocol CLI (Release -O3)...
g++ -std=c++20 -O3 -s -I core/include -I . cli/main.cpp cli/tui.cpp core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp core/src/sync_engine.cpp core/src/multi_engine.cpp core/src/routing_rules.cpp providers/azure/azure_provider.cpp providers/common/http_client.cpp providers/dropbox/dropbox_provider.cpp providers/google_drive/gdrive_provider.cpp providers/http/http_provider.cpp providers/icloud/icloud_provider.cpp providers/local/local_provider.cpp providers/onedrive/onedrive_provider.cpp providers/s3/s3_provider.cpp providers/webdav/webdav_provider.cpp -o utp_release.exe -lcurl -lssl -lcrypto -lws2_32 -lsqlite3
if %errorlevel% neq 0 (
    echo Compilation Failed! Please ensure you have MSYS2 with gcc, libcurl, sqlite3, and openssl installed.
    exit /b %errorlevel%
) else (
    if exist utp.exe move /y utp.exe utp_old.exe >nul 2>&1
    move /y utp_release.exe utp.exe >nul 2>&1
    if exist utp_old.exe del /f /q utp_old.exe >nul 2>&1
    echo Compilation Successful! Created optimized release binary: utp.exe
    echo Run visual interactive transfer: .\utp.exe transfer
)
