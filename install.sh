#!/usr/bin/env bash
# Universal Cloud Transfer Protocol (UTP) - One-Line Quick Installer
set -e

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

REPO="ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol"
VERSION="v2.6.1"

echo -e "\n${BOLD}${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║       Installing Universal Cloud Transfer Protocol (UTP)     ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}\n"

# 1. Detect OS and Architecture
OS="$(uname -s)"
ARCH="$(uname -m)"
ASSET_NAME=""

if [ "$OS" = "Linux" ]; then
    if [ "$ARCH" = "x86_64" ]; then
        ASSET_NAME="utp-linux-x64.tar.gz"
    else
        echo -e "${RED}[Error] Architecture '$ARCH' is not yet supported for prebuilt Linux binaries.${NC}"
        echo -e "Please build from source using: git clone && ./build.sh"
        exit 1
    fi
elif [ "$OS" = "Darwin" ]; then
    ASSET_NAME="utp-macos.tar.gz"
else
    echo -e "${RED}[Error] Unsupported OS: $OS${NC}"
    echo -e "On Windows, please use: scoop install utp  OR  npx utp-engine"
    exit 1
fi

DOWNLOAD_URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET_NAME}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo -e "==> Downloading ${BOLD}${VERSION}${NC} for ${BOLD}${OS} (${ARCH})${NC}..."
if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$DOWNLOAD_URL" -o "$TMP_DIR/utp.tar.gz"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$DOWNLOAD_URL" -O "$TMP_DIR/utp.tar.gz"
else
    echo -e "${RED}[Error] Neither 'curl' nor 'wget' was found.${NC}"
    exit 1
fi

echo -e "==> Extracting binary..."
tar -xzf "$TMP_DIR/utp.tar.gz" -C "$TMP_DIR"

# 2. Determine Installation Target Directory
INSTALL_DIR="/usr/local/bin"
USE_SUDO=false

if [ ! -w "$INSTALL_DIR" ]; then
    if [ "$EUID" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
        USE_SUDO=true
    else
        INSTALL_DIR="$HOME/.local/bin"
        mkdir -p "$INSTALL_DIR"
    fi
fi

echo -e "==> Installing to ${BOLD}${INSTALL_DIR}/utp${NC}..."
if [ "$USE_SUDO" = true ]; then
    sudo mv "$TMP_DIR/utp" "$INSTALL_DIR/utp"
    sudo chmod 755 "$INSTALL_DIR/utp"
else
    mv "$TMP_DIR/utp" "$INSTALL_DIR/utp"
    chmod 755 "$INSTALL_DIR/utp"
fi

# 3. Check PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo -e "\n${BOLD}Note:${NC} Please ensure ${BOLD}${INSTALL_DIR}${NC} is in your PATH."
    echo -e "Add this to your ~/.bashrc or ~/.zshrc:  ${CYAN}export PATH=\"$INSTALL_DIR:\$PATH\"${NC}"
fi

echo -e "\n${GREEN}✔ Universal Cloud Transfer Protocol (UTP) installed successfully!${NC}"
echo -e "Run ${BOLD}utp transfer${NC} to launch the interactive cloud drive manager.\n"
