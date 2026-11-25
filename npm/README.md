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
[![Official Portal](https://img.shields.io/badge/Official%20Site-ctp.arpitnayak.tech-FF5B1F.svg)](https://ctp.arpitnayak.tech)
[![Privacy & Terms](https://img.shields.io/badge/Legal-Privacy%20%26%20Terms-22c55e.svg)](https://ctp.arpitnayak.tech/privacy)
[![C++20](https://img.shields.io/badge/Standard-C%2B%2B20-purple.svg)](https://en.cppreference.com/w/cpp/20)
[![Release](https://img.shields.io/badge/Release-v2.6.1--pro-cyan.svg)](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases)
[![Platforms](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-emerald.svg)](#installation--quick-start)
[![Zero Disk Footprint](https://img.shields.io/badge/Storage-Zero%20Disk%20RAM%20Stream-orange.svg)](#-architecture--security)
[![Encryption](https://img.shields.io/badge/Security-TLS%201.3%20%2F%20AES--256-cyan.svg)](#-architecture--security)
[![TypeScript SDK](https://img.shields.io/badge/TypeScript%20SDK-v2.6.1-3178C6.svg)](sdk/typescript)

</div>

---

## ⚡ What is UTP?

**UTP (Universal Transfer Protocol)** is a high-performance cloud migration, synchronization, and routing engine. Unlike traditional tools that download gigabytes of files to your local hard drive before re-uploading them to another provider, UTP streams bytes **directly from cloud-to-cloud through volatile RAM ring buffers over encrypted TLS 1.3 tunnels**.

- **0-Disk Footprint**: Never exhausts your local SSD/NVMe or incurs local disk wear when migrating multi-gigabyte or terabyte datasets.
- **Interactive Developer-Grade TUI**: Full visual cloud file explorer with breadcrumb directory navigation, multi-account OAuth management, folder creation, and recursive batch transfers.
- **Real-Time Live HUD**: Smooth gradient progress bars, real-time MB/s throughput velocity, rolling ETA calculation, and chunk-by-chunk state monitoring.
- **Fast-Path In-Flight Verification**: Computes dual SHA-256 and MD5 checksums in memory on the fly and matches them directly against provider cloud hashes (OneDrive `sha256Hash`, Google Drive `md5Checksum`, AWS S3 `ETag`), eliminating cloud read-back egress.
- **Resumable ACID Ledger**: Crash-resilient SQLite WAL-backed chunk ledger (`utp.db`). Interrupted transfers resume exactly from the last committed chunk.
- **Delta Folder Sync Engine**: High-speed folder synchronization with 0-byte transfer skips for unchanged files, 5 configurable conflict policies, and bidirectional sync.
- **Multi-Destination Fan-Out & Smart Routing**: Stream a source file to multiple cloud targets simultaneously in a single read pass, or dynamically route files based on file extension, file size, path prefix, or filename.
- **Background Daemon Management**: Integrated detached background process controls (`utp daemon start|stop|status`) with Bearer token authentication and REST API.
- **Ecosystem SDKs**: Native, strongly-typed TypeScript SDK (`@utp/client`) for Node.js/web and Python client (`utp-client`) for automation.

---

## ⚖️ Surface Maturity & Status Matrix

UTP adheres to strict MVP scope discipline (§12 of `CLAUDE.md`). Surfaces are explicitly classified into **Core Production MVP** (which undergo automated CI PR gates including `kill -9` → resume → SHA-256 verification) and **Experimental Post-MVP Extensions**:

| Surface | Component / Scope | Maturity Status | Verification Gate |
| :--- | :--- | :--- | :--- |
| **Local Storage** | `file://` provider | **Core MVP (Production)** | Automated CI + DoD Resume Verified |
| **Google Drive** | `gdrive://` provider | **Core MVP (Production)** | OAuth2 PKCE + Token Refresh + DoD Verified |
| **OneDrive** | `onedrive://` provider | **Core MVP (Production)** | OAuth2 + Large Upload Session Verified |
| **CLI & TUI Engine** | `utp transfer`, `resume`, `pause`, `status`, `cancel`, `list` | **Core MVP (Production)** | End-to-End Terminal UI & SHA-256 Match |
| **Folder Sync Engine** | `utp sync <src> <dest>` | **Core MVP (Production)** | Phase 6 Delta Change Detection (0 bytes skip) + 5 Conflict Policies |
| **Multi-Destination Fan-Out** | `utp route <src> <d1> <d2>...` | **Core MVP (Production)** | Phase 7 Zero-Disk Fan-Out Streaming + Concurrent Broadcast |
| **Smart Routing Engine** | `utp route <src> --rule "<c>:<d>"` | **Core MVP (Production)** | Phase 10 Pattern Engine (ext, size, prefix, name, default fallback) |
| **Daemon Management** | `utp daemon start\|stop\|status` | **Core MVP (Production)** | Phase 9 Detached Background Process Group Controls |
| **TypeScript / Node SDK** | `@utp/client` (`sdk/typescript/`) | **Core MVP (Production)** | Phase 8 Native Typed Client + Async Event Stream + Mock Battery |
| **State Persistence** | SQLite WAL ledger (`utp.db`) | **Core MVP (Production)** | Crash-Resilient Upsert & Event Stream |
| **Daemon Server (`utpd`)** | REST API (`/api/transfers`) | *Experimental (Post-MVP)* | Marked `X-UTP-Surface: experimental` |
| **Web Dashboard** | Browser UI (`web/`) | *Experimental (Post-MVP)* | Preview UI for Daemon API |
| **Python SDK** | `utp-client` (`sdk/python/`) | *Experimental (Post-MVP)* | PyPI Beta Client Library |
| **Extended Clouds** | `s3://`, `r2://`, `b2://`, `azure://`, `dropbox://`, `icloud://`, `webdav://` | *Experimental (Post-MVP)* | Pending Automated Multi-Cloud DoD Gates |

---

## 🌐 Supported Protocols

### Core Production MVP
| Storage Provider | URI Scheme | Features |
| :--- | :--- | :--- |
| **Local Storage** | `file://<path>` | Local NVMe, SSD, and mounted network drives |
| **Google Drive** | `gdrive://<id>` | Multi-account OAuth2, resumable uploads, server-side copy |
| **Microsoft OneDrive** | `onedrive://<path>` | Personal OneDrive, Microsoft 365 & SharePoint |

### Extended Cloud Adapters (Experimental Post-MVP)
| Storage Provider | URI Scheme | Features |
| :--- | :--- | :--- |
| **Dropbox** | `dropbox://<path>` | Dropbox v2 API, chunked upload sessions (`files/copy_v2`) |
| **Apple iCloud** | `icloud://<path>` | Direct Web API (`docws.icloud.com`) + Local CloudKit Sync Bridge |
| **Amazon S3** | `s3://<bucket>/<key>` | Multipart uploads, SigV4 signing, all AWS regions |
| **Cloudflare R2** | `r2://<bucket>/<key>` | High-performance S3 API with zero egress fees |
| **Backblaze B2** | `b2://<bucket>/<key>` | Cost-effective B2 cloud storage endpoints |
| **Azure Blob** | `azure://<container>/<blob>` | Microsoft Azure block blobs & containers |
| **WebDAV / Nextcloud** | `webdav://<path>` | RFC 4918 compliant: Nextcloud, ownCloud, TrueNAS, Synology NAS |

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
Launch the visual interactive terminal interface with arrow-key navigation and live drive explorer:
```bash
utp transfer
```
- **Account Selection & Management**: Seamlessly authenticate and switch between multiple Google Drive and OneDrive accounts.
- **Directory Browsing**: Navigate folders using `cd <#>`, go up with `cd ..`, and paginate with `p` (previous) / `n` (next).
- **Selection Modes**: Select a specific file by number, enter a folder, or type `all` to batch transfer entire directories recursively.
- **Target Folder Creation**: Choose destination directories with inline breadcrumb navigation or create new folders on the fly.
- **Real-Time HUD**: Watch live transfer speeds (MB/s), ETA countdowns, and gradient progress bars, followed by an execution summary.

---

### 2. Direct CLI Pipeline
Stream directly between any two cloud endpoints in a single command without touching local storage:
```bash
# Google Drive to Microsoft OneDrive
utp transfer gdrive://1BxiMVs0XRA5nFMdKvBHKr... onedrive://Backups/data.zip

# Dropbox to Amazon S3
utp transfer dropbox://Dataset/archive.tar.gz s3://my-bucket/archive.tar.gz

# Cloudflare R2 to Local Storage (with 20 MB/s bandwidth limit)
utp transfer r2://assets/video.mp4 file://D:/video.mp4 --bandwidth 20971520

# With deterministic ID for programmatic tracking and resumption
utp transfer gdrive://1GwVK676IBt... onedrive://Archives/dataset.tar.gz --id migration-job-42
```

---

### 3. Folder Synchronization & Delta Sync
Synchronize directory trees recursively between any two providers with delta change detection (unchanged files transfer **0 bytes**):
```bash
# Unidirectional mirror from local directory to Google Drive
utp sync file://my_project gdrive://backups/my_project

# Bidirectional two-way synchronization
utp sync file://docs onedrive://Documents -b

# Dry-run preview of planned sync actions without modifying destination
utp sync file://data s3://bucket/data -n

# Specify conflict policy: OVERWRITE (default), FAIL, RENAME, SKIP, or NEWER_WINS
utp sync file://data s3://bucket/data --policy newer_wins
```

---

### 4. Multi-Destination Routing & Fan-Out Streaming
Stream a single source file to multiple cloud destinations simultaneously via shared memory fan-out buffers in a single read pass:
```bash
# Stream from AWS S3 simultaneously to Google Drive AND OneDrive
utp route s3://bucket/dataset.tar.gz gdrive://backups/dataset.tar.gz onedrive://archives/dataset.tar.gz

# Fault-tolerant fan-out: surviving destinations continue even if one fails
utp route file://firmware.bin s3://primary/firmware.bin b2://backup/firmware.bin --policy best_effort
```

---

### 5. Rules-Based Smart Routing Engine
Dynamically route files matching extensions, byte sizes, prefixes, or keywords to different storage destinations:
```bash
# Route media to S3, documents to OneDrive, and archives (>100MB) to Backblaze B2
utp route ./data \
  --rule "ext=mp4,mkv,mov:s3://media-vault/" \
  --rule "ext=pdf,docx:onedrive://documents/" \
  --rule "size>100M:b2://cold-archive/" \
  --default "file://fallback_dir/"
```

Supported rule conditions:
- `ext=mp4,mkv,mov`: Matches file extension (comma-separated list).
- `size>100M`, `size<10K`, `size>=1G`: Evaluates byte size with standard metric suffixes (`K`, `M`, `G`, `T`).
- `path=documents/`: Matches directory prefix in relative path.
- `name=archive`: Matches exact file stem or substring.

---

### 6. Transfer Lifecycle Management
All transfers are recorded in the local SQLite ledger (`utp.db`) and can be inspected, paused, resumed, or cancelled at any time:

```bash
# Inspect real-time state, chunk completion, and recent events
utp status <transfer_id>

# Resume an interrupted or paused transfer exactly where it left off
utp resume <transfer_id>

# Safely pause an active transfer at the next chunk boundary
utp pause <transfer_id>

# Abort an active transfer and discard partial state
utp cancel <transfer_id>

# View recent transfer history recorded in the ACID ledger
utp list
```

---

### 7. Background API Daemon Lifecycle
Manage the background REST API server directly from the CLI:
```bash
# Start background API server with detached process group and bearer auth
utp daemon start --port 8080

# Query daemon runtime state, active PID, endpoint, and token
utp daemon status

# Gracefully terminate background daemon process
utp daemon stop
```

---

### 8. Native TypeScript SDK (`@utp/client`)
Integrate zero-disk cloud transfers into Node.js, Next.js, and backend services:
```typescript
import { UTPClient } from '@utp/client';

const client = new UTPClient({
  baseUrl: 'http://127.0.0.1:8080',
  token: 'YOUR_API_TOKEN'
});

// Launch zero-disk streaming transfer
const transfer = await client.transfer({
  src: 'gdrive://1GwVK676IBt10iBEdRzh2LgqWdajT-In_',
  dest: 'onedrive://Backups/dataset.tar.gz'
});

// Await completion with real-time progress callbacks
await client.waitForCompletion(transfer.id, { timeoutMs: 60000 }, (p) => {
  console.log(`Progress: ${p.bytesTransferred} / ${p.size} bytes (${p.progressPercent.toFixed(1)}%)`);
});
```

---

### 9. Python SDK (`utp-client`)
Automate data pipelines with Python:
```python
from utp import UTPClient

client = UTPClient(base_url="http://127.0.0.1:8080", token="YOUR_API_TOKEN")

# Trigger transfer
transfer = client.transfer(
    src="s3://source-bucket/model.onnx",
    dest="onedrive://AI_Models/model.onnx"
)

# Monitor status
status = client.wait_for_completion(transfer["id"])
print(f"Transfer finished with status: {status['state']}")
```

---

### 10. CLI Quick Reference

| Command | Arguments | Flags | Description |
| :--- | :--- | :--- | :--- |
| `utp transfer` | *(none)* | *(none)* | Launch interactive visual TUI explorer |
| `utp transfer` | `<src> <dest>` | `--bandwidth`, `--id`, `--verbose` | Direct cloud-to-cloud RAM streaming |
| `utp sync` | `<src> <dest>` | `-b`, `-n`, `--policy` | Recursive delta folder synchronization |
| `utp route` | `<src> <d1> <d2>...` | `--policy` | Multi-destination fan-out broadcast |
| `utp route` | `<src>` | `--rule`, `--default` | Rules-based smart routing engine |
| `utp status` | `<transfer_id>` | *(none)* | Inspect progress, chunk states, and event log |
| `utp resume` | `<transfer_id>` | `--bandwidth` | Resume interrupted transfer from SQLite ledger |
| `utp pause` | `<transfer_id>` | *(none)* | Gracefully pause transfer between chunks |
| `utp cancel` | `<transfer_id>` | *(none)* | Abort transfer and clean up partial state |
| `utp list` | *(none)* | *(none)* | List recent transfers and execution status |
| `utp daemon` | `start\|stop\|status` | `--port` | Manage background REST API server process |

---

## 🛡️ Architecture & Security

```
   ┌────────────────────────────────────────────────────────────────────────┐
   │                          UTP Core Engine v2.6                          │
   │  ┌───────────────────────┐                  ┌───────────────────────┐  │
   │  │   Memory RAM Ring     │◀────────────────▶│    Chunk Planner      │  │
   │  │   (Zero Local Disk)   │                  │   (Adaptive 2.5–40MB) │  │
   │  └───────────────────────┘                  └───────────────────────┘  │
   │              │                                           │             │
   │              ▼                                           ▼             │
   │  ┌───────────────────────┐                  ┌───────────────────────┐  │
   │  │ Dual In-Flight Hash   │                  │ Server-Side Fast-Path │  │
   │  │ (SHA-256 + MD5 stream)│                  │ (OneDrive/GDrive/S3)  │  │
   │  └───────────────────────┘                  └───────────────────────┘  │
   │              │                                           │             │
   │              └───────────────────┬───────────────────────┘             │
   │                                  ▼                                     │
   │             ┌────────────────────┴────────────────────┐                │
   │             ▼                                         ▼                │
   │   ┌───────────────────┐                     ┌───────────────────┐      │
   │   │  Source Provider  │                     │   Dest Provider   │      │
   │   │(HTTP Range Stream)│                     │ (Chunked Session) │      │
   │   └───────────────────┘                     └───────────────────┘      │
   └────────────────────────────────────────────────────────────────────────┘
                                      ▲
                                      │ WAL Mode
   ┌──────────────────────────────────┴─────────────────────────────────────┐
   │                SQLite State Tracking Ledger (`utp.db`)                 │
   └────────────────────────────────────────────────────────────────────────┘
```

- **Client-Side Encryption**: All transfers utilize direct TLS 1.3 encrypted sockets with modern ciphers.
- **Fast-Path Integrity**: Eliminates cloud download read-back by matching in-flight SHA-256/MD5 hashes directly against provider metadata.
- **Credential Hygiene**: Access tokens and refresh credentials are stored strictly in local user configurations (`.utp_*_token.json`) protected by standard OS file permissions. No third-party servers, intermediate proxies, or telemetry services ever touch your tokens or data.

---

## 🛠️ Building from Source

### Prerequisites
- C++20 compliant compiler (`g++` >= 11, `clang` >= 13, or MSVC 2022)
- `libcurl`, `OpenSSL` (libssl & libcrypto), and `sqlite3`

### Using Build Scripts (Recommended)
```bash
# Windows (MSYS2 UCRT64 / MinGW)
.\build_cli.bat

# Linux / macOS
chmod +x ./build.sh && ./build.sh
```

### Using CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Running Test Suites
```bash
# Build and run the test battery (Windows)
.\build_tests.bat
.\test_runner.exe

# Or via CMake / CTest
cd build && ctest --output-on-failure
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
