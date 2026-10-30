@echo off
set PATH=C:\msys64\usr\bin;C:\msys64\ucrt64\bin;%PATH%
echo Compiling Cloud Transfer Protocol Test Runner...
g++ -std=c++20 -O3 -I core/include -I . tests/test_runner.cpp core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp core/src/sync_engine.cpp core/src/multi_engine.cpp providers/common/http_client.cpp providers/local/local_provider.cpp -o test_runner.exe -lcurl -lssl -lcrypto -lsqlite3 -lws2_32
if %errorlevel% neq 0 (
    echo Test Runner Compilation Failed!
    exit /b %errorlevel%
) else (
    echo Test Runner Compilation Successful! Created test_runner.exe
)
