# Universal Cloud Transfer Protocol (UTP)

<div align="center">

```
   __  ___________   ______            _            
  / / / /_  __/ __ \ / ____/___  ____ _(_)___  ___  
 / / / / / / / /_/ // __/ / __ \/ __ `/ / __ \/ _ \ 
/ /_/ / / / / ____// /___/ / / / /_/ / / / / /  __/ 
\____/ /_/ /_/    /_____/_/ /_/\__, /_/_/ /_/\___/  
                              /____/                
```

**Direct Zero-Disk Memory Pipeline for Personal, Commercial & Enterprise Cloud Storage**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-purple.svg)](https://en.cppreference.com/w/cpp/20)
[![Platforms](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-emerald.svg)](#installation)
[![Zero Disk Footprint](https://img.shields.io/badge/Storage-Zero%20Disk%20RAM%20Stream-orange.svg)](#architecture)
[![Encryption](https://img.shields.io/badge/Security-TLS%201.3%20%2F%20AES--256-cyan.svg)](#security)

</div>

---

## ⚡ What is UTP?

**UTP (Universal Transfer Protocol)** is a high-performance cloud migration and synchronization engine. Unlike traditional tools that download gigabytes of files to your local hard drive before uploading them to another cloud, UTP streams bytes **directly from cloud-to-cloud through volatile RAM buffers over encrypted TLS 1.3 tunnels**.

- **0-Disk Footprint**: Never fills your local NVMe/SSD when migrating 100GB+ datasets.
- **Interactive Keyboard TUI**: Full visual file explorer with directory browsing, recursive folder transfers, and pagination.
- **Resumable State Engine**: Crash-resilient SQLite-backed chunk ledger (`utp.db`). Pick up interrupted transfers exactly where they left off.
- **Unified 10-Cloud Ecosystem**: Connect personal consumer drives, enterprise S3 object storage, and self-hosted WebDAV servers.

---

## 🌐 Supported Protocols

| Storage Provider | URI Scheme | Features |
| :--- | :--- | :--- |
| **Google Drive** | `gdrive://<id>` | Multi-account OAuth2, resumable uploads, server-side copy |
| **Microsoft OneDrive** | `onedrive://<path>` | Personal OneDrive, Microsoft 365 & SharePoint |
| **Dropbox** | `dropbox://<path>` | Dropbox v2 API, chunked upload sessions (`files/copy_v2`) |
| **Apple iCloud** | `icloud://<path>` | Dual-Mode: Direct Web (`docws.icloud.com`) + Local Sync Bridge |
| **Amazon S3** | `s3://<bucket>/<key>` | Multipart uploads, SigV4 signing, all AWS regions |
| **Cloudflare R2** | `r2://<bucket>/<key>` | High-performance S3 API with zero egress fees |
| **Backblaze B2** | `b2://<bucket>/<key>` | Cost-effective B2 cloud storage endpoints |
| **Azure Blob** | `azure://<container>/<blob>` | Microsoft Azure block blobs & containers |
| **WebDAV / Nextcloud** | `webdav://<path>` | RFC 4918 compliant: Nextcloud, ownCloud, TrueNAS, Synology |
| **Local Storage** | `file://<path>` | Local NVMe, SSD, and mounted network drives |

---

## 🚀 Installation & Quick Start

### Option 1: Universal 1-Line Installer (Linux & macOS)
```bash
curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash
```

### Option 2: Instant Run with `npx` (No installation needed)
```bash
npx utp-engine transfer
```

### Option 3: Package Managers
```bash
# macOS / Linux (Homebrew)
brew install ARPIT-NAYAK-LEGEND/utp/utp
# Or directly from the formula URL:
# brew install https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/packaging/homebrew/utp.rb

# Arch Linux (AUR / Pacman)
yay -S utp-bin
# Or build locally:
# cd packaging/pacman && makepkg -si

# Windows (Scoop)
scoop install https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/packaging/scoop/utp.json

# NPM (Global)
npm install -g utp-engine
```

### Option 4: Standalone Prebuilt Binaries (GitHub Releases)
Download prebuilt executables directly from [Releases](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases):
- **Windows**: `utp-windows-x64.zip` (`utp.exe`)
- **Linux**: `utp-linux-x64.tar.gz` (`utp`)
- **macOS**: `utp-macos.tar.gz` (`utp`)

---

## 🎮 Usage

### 1. Interactive Visual TUI (Recommended)
Launch the visual TUI with arrow-key navigation and live drive explorer:
```bash
utp transfer
```
- Navigate files and directories with `cd <#>`, `p` (prev page), `n` (next page).
- Select single files by number, type `all` to batch transfer, or select entire folders recursively.

### 2. Direct CLI Pipeline
Stream directly between any two cloud endpoints in one command:
```bash
# Google Drive to Microsoft OneDrive
utp transfer gdrive://1BxiMVs0XRA5nFMdKvBHKr... onedrive://Backups/data.zip

# Dropbox to Amazon S3
utp transfer dropbox://Dataset/archive.tar.gz s3://my-bucket/archive.tar.gz

# Cloudflare R2 to Local NVMe (with 20 MB/s bandwidth limit)
utp transfer r2://assets/video.mp4 file://D:/video.mp4 --bandwidth 20971520

# Apple iCloud Drive to Nextcloud
utp transfer icloud://Documents/Report.pdf webdav://Shared/Report.pdf
```

### 3. Resume Paused Transfers
If a transfer is interrupted by network failure or reboot:
```bash
utp resume <transfer_id>
```

---

## 🛡️ Architecture & Security

```
   ┌─────────────────────────────────────────────────────────────┐
   │                       UTP Core Engine                       │
   │  ┌───────────────────────┐       ┌───────────────────────┐  │
   │  │   Memory RAM Ring     │◀─────▶│    Chunk Planner      │  │
   │  │   (Zero Local Disk)   │   │   (Adaptive 2.5–40MB)   │   │
   │  └───────────────────────┘       └───────────────────────┘  │
   │                             │                               │
   │             ┌───────────────┴───────────────┐               │
   │             ▼                               ▼               │
   │   ┌───────────────────┐           ┌───────────────────┐     │
   │   │  Source Provider  │           │   Dest Provider   │     │
   │   │ (HTTP Range Stream)           │ (Chunked Session) │     │
   │   └───────────────────┘           └───────────────────┘     │
   └─────────────────────────────────────────────────────────────┘
                                 ▲
                                 │ WAL Mode
   ┌─────────────────────────────┴───────────────────────────────┐
   │           SQLite State Tracking Ledger (`utp.db`)           │
   └─────────────────────────────────────────────────────────────┘
```

- **Client-Side Encryption**: Transfers utilize TLS 1.3 direct socket connections.
- **Credential Hygiene**: Tokens and accounts are stored locally with strict user permissions (`.utp_*_token.json`). No third-party servers ever see your credentials.

---

## 🛠️ Building from Source

### Prerequisites
- C++20 compliant compiler (`g++` >= 11, `clang` >= 13, or MSVC 2022)
- `libcurl`, `OpenSSL` (libssl & libcrypto), and `sqlite3`

### Using Build Scripts (Recommended)
```bash
# Windows (MSYS2 UCRT64)
.\build_cli.bat

# Linux / macOS
chmod +x ./build.sh && ./build.sh
```

### Using CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
