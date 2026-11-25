#!/usr/bin/env bash
# Universal Cloud Transfer Protocol (UTP) - Linux & macOS Build Script
set -e

# ANSI Color Codes
CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}==> Compiling Universal Cloud Transfer Protocol (UTP)...${NC}"

# Detect Compiler (clang++ on macOS, g++ on Linux)
if [ -z "$CXX" ]; then
    if command -v clang++ >/dev/null 2>&1 && [ "$(uname)" = "Darwin" ]; then
        CXX="clang++"
    elif command -v g++ >/dev/null 2>&1; then
        CXX="g++"
    else
        CXX="clang++"
    fi
fi

OS="$(uname -s)"
EXTRA_INCLUDES=""
EXTRA_LIBS=""

if [ "$OS" = "Darwin" ]; then
    # macOS Homebrew include & library discovery
    if command -v brew >/dev/null 2>&1; then
        OPENSSL_DIR="$(brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null || true)"
        SQLITE_DIR="$(brew --prefix sqlite 2>/dev/null || true)"
        CURL_DIR="$(brew --prefix curl 2>/dev/null || true)"

        [ -n "$OPENSSL_DIR" ] && EXTRA_INCLUDES="$EXTRA_INCLUDES -I$OPENSSL_DIR/include" && EXTRA_LIBS="$EXTRA_LIBS -L$OPENSSL_DIR/lib"
        [ -n "$SQLITE_DIR" ]  && EXTRA_INCLUDES="$EXTRA_INCLUDES -I$SQLITE_DIR/include"  && EXTRA_LIBS="$EXTRA_LIBS -L$SQLITE_DIR/lib"
        [ -n "$CURL_DIR" ]    && EXTRA_INCLUDES="$EXTRA_INCLUDES -I$CURL_DIR/include"    && EXTRA_LIBS="$EXTRA_LIBS -L$CURL_DIR/lib"
    fi
fi

$CXX -std=c++20 -O3 -I core/include -I . $EXTRA_INCLUDES \
    cli/main.cpp cli/tui.cpp cli/stream_server.cpp \
    core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp \
    core/src/sync_engine.cpp core/src/multi_engine.cpp core/src/routing_rules.cpp \
    providers/azure/azure_provider.cpp providers/common/http_client.cpp \
    providers/dropbox/dropbox_provider.cpp providers/google_drive/gdrive_provider.cpp \
    providers/http/http_provider.cpp providers/icloud/icloud_provider.cpp providers/local/local_provider.cpp \
    providers/onedrive/onedrive_provider.cpp providers/s3/s3_provider.cpp \
    providers/webdav/webdav_provider.cpp \
    -o utp $EXTRA_LIBS -lcurl -lssl -lcrypto -lpthread -lsqlite3

echo -e "${GREEN}==> Compilation Successful! Created './utp'${NC}"
echo -e "You can now run: ./utp transfer gdrive://FILE_ID onedrive://DEST_PATH"
