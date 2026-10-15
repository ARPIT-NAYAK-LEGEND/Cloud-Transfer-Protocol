@echo off
set PATH=C:\msys64\usr\bin;C:\msys64\ucrt64\bin;%PATH%
echo Compiling Cloud Transfer Protocol CLI...
g++ -std=c++20 -I core/include -I . cli/main.cpp cli/tui.cpp core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp providers/azure/azure_provider.cpp providers/common/http_client.cpp providers/google_drive/gdrive_provider.cpp providers/local/local_provider.cpp providers/onedrive/onedrive_provider.cpp providers/s3/s3_provider.cpp -o utp.exe -lcurl -lssl -lcrypto -lws2_32 -lsqlite3
if %errorlevel% neq 0 (
    echo Compilation Failed! Please ensure you have MSYS2 with gcc, libcurl, sqlite3, and openssl installed.
) else (
    echo Compilation Successful! Created utp.exe
    echo You can now test it using: .\utp.exe transfer gdrive://FILE_ID gdrive://FILE_NAME
)
