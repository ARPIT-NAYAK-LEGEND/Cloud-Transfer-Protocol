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
[![Release](https://img.shields.io/badge/Release-v2.6.2--pro-cyan.svg)](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases)
[![Platforms](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-emerald.svg)](#installation--quick-start)
[![Zero Disk Footprint](https://img.shields.io/badge/Storage-Zero%20Disk%20RAM%20Stream-orange.svg)](#-architecture--security)
[![Encryption](https://img.shields.io/badge/Security-TLS%201.3%20%2F%20AES--256-cyan.svg)](#-architecture--security)
[![TypeScript SDK](https://img.shields.io/badge/TypeScript%20SDK-v2.6.2-3178C6.svg)](sdk/typescript)

</div>

---

## ⚡ What is UTP?

**UTP (Universal Transfer Protocol)** is an open-source, high-performance cloud migration, direct web ingestion, media streaming, and routing engine built in C++20. Unlike traditional tools that download gigabytes of files to your local hard drive before re-uploading them to another provider, UTP streams bytes **directly through dynamically sized RAM ring buffers over encrypted TLS 1.3 sockets**.

- **0-Disk Footprint**: Never exhausts your local SSD/NVMe or incurs local disk wear when migrating multi-gigabyte datasets or streaming remote media.
- **Direct Web URL Ingestion (`utp ingest` / `https://`)**: Stream direct HTTP/HTTPS downloads, ISOs, videos, and seedbox URLs straight into Google Drive, OneDrive, S3, R2, or local storage with automatic `HEAD` / `Range: bytes=0-0` probing and `Content-Disposition` filename detection.
- **Zero-Disk On-Demand Media Streaming (`utp stream` / `utp play`)**: Instantly expose any cloud file over a local HTTP/1.1 `206 Partial Content` range server (`http://127.0.0.1:8080/stream`) for lag-free seeking in VLC, MPV, or web browsers — plus authenticated `/api/stream` streaming & direct downloads in `utpd`.
- **Hardware-Adaptive Memory Chunking**: Dynamically queries physical system RAM and adapts chunk sizes to available memory, bandwidth, and provider limits.
- **Interactive Developer-Grade TUI**: Full operations hub with visual cloud file explorer, breadcrumb directory navigation, multi-account OAuth management, folder creation, direct download, recursive batch transfers, and instant stream actions.
- **Real-Time Live HUD**: Gradient progress bars, smoothed MB/s throughput velocity, rolling ETA calculation, and in-flight byte counters.
- **Fast-Path In-Flight Verification**: Computes dual SHA-256 and MD5 checksums in memory on the fly and matches them directly against provider cloud hashes (`sha256Hash`, `md5Checksum`, `ETag`).
- **Resumable ACID Ledger**: Crash-resilient SQLite WAL-backed chunk ledger (`utp.db`). Interrupted transfers resume cleanly from the last committed chunk.
- **Delta Folder Sync & Multi-Destination Routing**: Bidirectional/unidirectional folder sync with 5 conflict policies, concurrent multi-destination fan-out broadcast, and rule-based routing.
- **Background Daemon & SDKs**: Detached `utpd` REST API server (`utp daemon start|stop|status`) with Bearer auth, TypeScript SDK (`@utp/client`), and Python SDK (`utp-client`).

---

## ⚖️ Surface Maturity & Status Matrix

UTP classifies surfaces into **Core Production MVP** (covered by automated CI build/test gates including `kill -9` -> resume -> SHA-256 verification) and **Experimental Extensions**:

| Surface | Component / Scope | Maturity Status | Verification Gate |
| :--- | :--- | :--- | :--- |
| **Local Storage** | `file://` provider | **Core MVP (Production)** | Automated CI + DoD Resume Verified |
| **Google Drive** | `gdrive://` provider | **Core MVP (Production)** | OAuth2 PKCE + Token Refresh + DoD Verified |
| **OneDrive** | `onedrive://` provider | **Core MVP (Production)** | OAuth2 + Large Upload Session Verified |
| **CLI & TUI Engine** | `transfer`, `ingest`, `stream`, `play`, `resume`, `pause`, `status`, `cancel`, `list` | **Core MVP (Production)** | End-to-End Terminal UI & SHA-256 Match |
| **Folder Sync Engine** | `utp sync <src> <dest>` | **Core MVP (Production)** | Delta Change Detection (0 bytes skip) + 5 Conflict Policies |
| **Multi-Destination Fan-Out** | `utp route <src> <d1> <d2>...` | **Core MVP (Production)** | Zero-Disk Fan-Out Streaming + Concurrent Broadcast |
| **Smart Routing Engine** | `utp route <src> --rule "<c>:<d>"` | **Core MVP (Production)** | Pattern Engine (`ext`, `size`, `path`, `name`, `--default`) |
| **Daemon Management** | `utp daemon start\|stop\|status` | **Core MVP (Production)** | Detached Process Group Controls + PID/Token State |
| **TypeScript / Node SDK** | `@utp/client` (`sdk/typescript/`) | **Core MVP (Production)** | Native Typed Client + Async Event Stream + Unit Suite (10/10) |
| **State Persistence** | SQLite WAL ledger (`utp.db`) | **Core MVP (Production)** | Crash-Resilient Upsert & Event Stream |
| **Direct Web Ingestion** | `http://` & `https://` (`HttpProvider`) | *Experimental (Extension)* | Ranged HTTP Stream Read + Header Filename Probing |
| **Zero-Disk Stream Server** | Local `utp stream` & Daemon `/api/stream` | *Experimental (Extension)* | HTTP 206 Range Seeking + Token Auth |
| **Daemon Server (`utpd`)** | REST API (`/api/transfers`, `/api/stream`) | *Experimental (Post-MVP)* | Marked `X-UTP-Surface: experimental` |
| **Web Dashboard** | Browser UI (`web/`) | *Experimental (Post-MVP)* | Preview UI for Daemon API |
| **Python SDK** | `utp-client` (`sdk/python/`) | *Experimental (Post-MVP)* | Beta Client Library |
| **Extended Clouds** | `s3://`, `r2://`, `b2://`, `azure://`, `dropbox://`, `icloud://`, `webdav://` | *Experimental (Post-MVP)* | Pending Automated Multi-Cloud DoD Gates |

---

## 🌐 Supported Protocols

### Core Production MVP
| Storage Provider | URI Scheme | Features |
| :--- | :--- | :--- |
| **Local Storage** | `file://<path>` | Local NVMe, SSD, mounted disks, and direct cloud-to-local downloads |
| **Google Drive** | `gdrive://<id>` | Multi-account OAuth2, resumable uploads, server-side copy |
| **Microsoft OneDrive** | `onedrive://<path>` | Personal OneDrive, Microsoft 365 & SharePoint |

### Extended Adapters & Web Ingestion (Experimental)
| Storage Provider | URI Scheme | Features |
| :--- | :--- | :--- |
| **Direct Web URLs** | `https://<url>`, `http://<url>` | Read-only ranged HTTP ingestion from web downloads & seedboxes |
| **Dropbox** | `dropbox://<path>` | Dropbox v2 API, chunked upload sessions |
| **Apple iCloud** | `icloud://<path>` | Direct Web API (`docws.icloud.com`) + Local CloudKit Sync Bridge |
| **Amazon S3** | `s3://<bucket>/<key>` | Multipart uploads, SigV4 signing, all AWS regions |
| **Cloudflare R2** | `r2://<bucket>/<key>` | S3-compatible API with zero egress fees |
| **Backblaze B2** | `b2://<bucket>/<key>` | Cost-effective B2 S3 cloud endpoints |
| **Azure Blob** | `azure://<container>/<blob>` | Microsoft Azure block blobs & containers |
| **WebDAV / Nextcloud** | `webdav://<path>` | RFC 4918 compliant: Nextcloud, ownCloud, TrueNAS, Synology NAS |

---

## 🚀 Installation & Quick Start

### Option 1: Universal 1-Line Installer (Linux & macOS)
```bash
curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash
```

### Option 2: Instant Run with `npx` (No global install needed)
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
- **Windows**: `utp-windows-x64.zip` (`utp.exe`, `utpd.exe`)
- **Linux**: `utp-linux-x64.tar.gz` (`utp`, `utpd`)
- **macOS**: `utp-macos.tar.gz` (`utp`, `utpd`)

---

## 🎮 Usage

### 1. Interactive Visual TUI (Operations Hub)
Launch the interactive terminal hub (`utp`) or jump directly into the transfer explorer:
```bash
utp
# or directly open source/destination explorer:
utp transfer
```
- **Operations Hub**: Switch between Interactive Transfer & Web Ingestion, On-Demand Cloud Media Streamer, and Ledger History without leaving the session.
- **Direct Web URL & Cloud Download**: Choose **Direct Web / URL Stream (`https://`)** to pipe remote URLs straight into cloud storage, or select **Local Filesystem (`file://`)** as destination for direct cloud-to-local downloads.
- **Instant Stream & Play**: Select any file in the cloud explorer to choose **"Stream & Play directly (Zero-Disk)"**, or pick **"Stream & Play (Zero-Disk)" (`stream://`)** as destination.
- **Account Selection & Directory Browsing**: Manage multiple Google Drive, OneDrive, Dropbox, and iCloud accounts; browse with `cd <#>`, `cd ..`, `.` (pick current folder), `n`/`p` pagination, and `all` batch selection.

---

### 2. Direct Web URL Ingestion (`utp ingest` / `https://`)
Stream public or Bearer-authenticated direct download links straight into cloud storage or local disk through RAM — automatically inspecting `Content-Length`, `Content-Range`, and `Content-Disposition` headers:
```bash
# Ingest a remote ISO or archive straight to Google Drive (auto-detects filename)
utp ingest https://example.com/releases/ubuntu.iso gdrive://ISOs/

# Works without https:// prefix via 'ingest'
utp ingest example.com/video.mp4 onedrive://Media/video.mp4

# Stream directly from HTTP URL to Amazon S3 or Cloudflare R2
utp transfer https://example.com/dataset.tar.gz r2://datasets/dataset.tar.gz
```

---

### 3. Zero-Disk On-Demand Cloud Media Streaming (`utp stream` / `utp play`)
Play video, audio, or PDFs stored in any cloud provider without downloading the file to disk first. UTP launches an HTTP/1.1 `206 Partial Content` Range server on `http://127.0.0.1:8080/stream` (automatically scanning fallback ports `port..port+19` if occupied) and streams 512 KB volatile RAM slices on demand:
```bash
# Launch interactive cloud picker -> start streaming server
utp stream

# Stream and automatically open default media player / browser
utp play

# Stream a specific cloud URI on a custom port
utp stream gdrive://1BxiMVs0XRA5nFMdKvBHKr... --port 9090 --open
```
Playback instructions for `vlc`, `mpv`, and web browsers are displayed immediately; press `q`, `Enter`, `Esc`, or `Ctrl+C` to stop streaming cleanly and return to the menu.

---

### 4. Direct CLI Transfer & Cloud-to-Local Download
Stream directly between any two endpoints in a single command without intermediate disk copies:
```bash
# Google Drive to Microsoft OneDrive
utp transfer gdrive://1BxiMVs0XRA5nFMdKvBHKr... onedrive://Backups/data.zip

# Dropbox to Amazon S3
utp transfer dropbox://Dataset/archive.tar.gz s3://my-bucket/archive.tar.gz

# Cloudflare R2 direct download to Local Storage (with 20 MB/s bandwidth cap)
utp transfer r2://assets/video.mp4 file://D:/Downloads/video.mp4 --bandwidth 20971520

# Deterministic ID for programmatic tracking and resumption
utp transfer gdrive://1GwVK676IBt... onedrive://Archives/dataset.tar.gz --id migration-job-42
```

---

### 5. Folder Synchronization & Delta Sync
Synchronize directory trees recursively between any two providers with delta change detection (unchanged files transfer **0 bytes**):
```bash
# Unidirectional mirror from local directory to Google Drive
utp sync file://my_project gdrive://backups/my_project

# Bidirectional two-way synchronization
utp sync file://docs onedrive://Documents -b

# Dry-run preview of planned sync actions without modifying destination
utp sync file://data s3://bucket/data -n

# Conflict policy: OVERWRITE (default), FAIL, RENAME, SKIP, or NEWER_WINS
utp sync file://data s3://bucket/data --policy newer_wins
```

---

### 6. Multi-Destination Routing & Fan-Out Streaming
Stream a single source file to multiple cloud destinations simultaneously via shared memory fan-out buffers in a single read pass:
```bash
# Stream from AWS S3 simultaneously to Google Drive AND OneDrive
utp route s3://bucket/dataset.tar.gz gdrive://backups/dataset.tar.gz onedrive://archives/dataset.tar.gz

# Fault-tolerant fan-out: surviving destinations continue even if one fails
utp route file://firmware.bin s3://primary/firmware.bin b2://backup/firmware.bin --policy best_effort
```

---

### 7. Rules-Based Smart Routing Engine
Dynamically route files matching extensions, byte sizes, prefixes, or names to different storage destinations:
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
- `size>100M`, `size<10K`, `size>=1G`: Evaluates byte size with metric suffixes (`K`, `M`, `G`, `T`).
- `path=documents/`: Matches directory prefix in relative path.
- `name=archive`: Matches exact file stem or substring.

---

### 8. Transfer Lifecycle Management
All transfers are recorded in the local SQLite ledger (`utp.db`) and can be inspected, paused, resumed, or cancelled at any time:
```bash
utp status <transfer_id>    # Inspect state, chunks & event timeline
utp resume <transfer_id>    # Resume interrupted/paused transfer from ACID ledger
utp pause <transfer_id>     # Pause cleanly at next chunk boundary
utp cancel <transfer_id>    # Abort transfer and discard partial state
utp list                    # View recent transfer history
```

---

### 9. Background API Daemon (`utpd`) & REST Streaming
Manage the background REST API server directly from the CLI:
```bash
utp daemon start --port 8080   # Start detached daemon & generate .utp_api_token
utp daemon status              # Inspect active PID, endpoint & token
utp daemon stop                # Gracefully terminate background daemon
```

Authenticated HTTP endpoints (bound to `127.0.0.1` by default; `Authorization: Bearer <token>`, `X-API-Key`, or signed `?ticket=` for `/api/stream`):
- `POST /api/transfers` — Initiate background cloud-to-cloud or HTTPS ingestion transfer
- `GET /api/transfers`, `GET /api/transfers/:id`, `GET /api/transfers/:id/events` — Inspect state & events
- `POST /api/transfers/:id/pause` | `/cancel` — Control active transfers
- `POST /api/stream-tickets` — Mint short-lived resource-scoped HMAC-SHA256 `?ticket=` URLs
- `GET /api/stream?uri=<cloud_uri>&ticket=<ticket>` *(also supports `?src=`, `?id=`, `/api/stream/:id`, `&download=true`, `&name=<filename>`)* — Zero-disk HTTP 206 media stream & direct attachment download

---

### 10. Native TypeScript SDK (`@utp/client`)
Integrate zero-disk cloud transfers and streaming URLs into Node.js, Next.js, and backend services:
```typescript
import { UTPClient } from '@utp/client';

const client = new UTPClient({
  baseUrl: 'http://127.0.0.1:8080',
  token: 'YOUR_API_TOKEN',
});

// Launch zero-disk streaming transfer
const transferId = await client.transfer(
  'gdrive://1GwVK676IBt10iBEdRzh2LgqWdajT-In_',
  'onedrive://Backups/dataset.tar.gz'
);

// Await completion with real-time progress callbacks
await client.waitForCompletion(transferId, {
  timeoutMs: 60000,
  onProgress: (t) => {
    const pct = t.size > 0 ? (t.bytes_transferred / t.size) * 100 : 0;
    console.log(`Progress: ${t.bytes_transferred} / ${t.size} bytes (${pct.toFixed(1)}%)`);
  },
});

// Generate on-demand streaming or direct download URL
const streamUrl = client.getStreamUrl('gdrive://movies/clip.mp4');
const downloadUrl = client.getStreamUrl('gdrive://movies/clip.mp4', {
  download: true,
  filename: 'clip.mp4',
});
```

---

### 11. Python SDK (`utp-client`)
Automate data pipelines and media streaming links with Python:
```python
from utp_client import UTPClient

client = UTPClient(host="http://127.0.0.1:8080")

# Trigger transfer
transfer_id = client.transfer(
    "s3://source-bucket/model.onnx",
    "onedrive://AI_Models/model.onnx"
)

# Monitor until completion
status = client.wait_until_complete(transfer_id)
print(f"Transfer finished with status: {status['state']}")

# Construct streaming or direct download URL
stream_url = client.get_stream_url("gdrive://movies/clip.mp4", download=False)
```

---

### 12. CLI Quick Reference

| Command | Arguments | Flags | Description |
| :--- | :--- | :--- | :--- |
| `utp` / `utp menu` | *(none)* | *(none)* | Interactive master operations menu |
| `utp transfer` | *(none)* | *(none)* | Launch interactive visual TUI explorer |
| `utp transfer` | `<src> <dest>` | `--bandwidth`, `--id`, `--verbose` | Direct cloud/web/local RAM streaming transfer |
| `utp ingest` | `<url> <dest>` | `--bandwidth`, `--id` | Direct HTTP/HTTPS URL ingestion to cloud or local |
| `utp stream` | `[cloud-uri]` | `--port`, `--open` (`-o`) | Zero-disk HTTP 206 media streaming server |
| `utp play` | `[cloud-uri]` | `--port` | Stream cloud media and auto-launch player |
| `utp sync` | `<src> <dest>` | `-b`, `-n`, `--policy` | Recursive delta folder synchronization |
| `utp route` | `<src> <d1> <d2>...` | `--policy`, `--bandwidth` | Multi-destination fan-out broadcast |
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
   │                        UTP Core Engine v2.6.2                          │
   │  ┌───────────────────────┐                  ┌───────────────────────┐  │
   │  │   Memory RAM Ring     │◀────────────────▶│ Adaptive ChunkPlanner │  │
   │  │   (Zero Local Disk)   │                  │ (RAM & Net Bounded)   │  │
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
   │   │  Source / HTTP    │                     │ Dest / HTTP 206   │      │
   │   │(Range Pull Stream)│                     │  Stream & Session │      │
   │   └───────────────────┘                     └───────────────────┘      │
   └────────────────────────────────────────────────────────────────────────┘
                                      ▲
                                      │ WAL Mode
   ┌──────────────────────────────────┴─────────────────────────────────────┐
   │                SQLite State Tracking Ledger (`utp.db`)                 │
   └────────────────────────────────────────────────────────────────────────┘
```

- **Client-Side Encryption**: All transfers utilize direct TLS 1.3 encrypted sockets with certificate verification.
- **Fast-Path Integrity**: Eliminates cloud download read-back by matching in-flight SHA-256/MD5 hashes directly against provider metadata.
- **Credential Hygiene**: Access tokens and refresh credentials are stored strictly in local user files (`.utp_*_token.json`) and daemon tokens in `.utp_api_token`. No third-party servers, intermediate proxies, or telemetry services ever touch your tokens or data.

---

## 🛠️ Building from Source

### Prerequisites
- C++20 compliant compiler (`g++` >= 11, `clang` >= 13, or MSVC 2022)
- `libcurl`, `OpenSSL` (`libssl` & `libcrypto`), and `sqlite3`

### Using Build Scripts (Recommended)
```bash
# Windows (MSYS2 UCRT64 / MinGW)
.\scripts\build_cli.bat
.\scripts\build_server.bat

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
.\scripts\build_tests.bat
.\test_runner.exe

# Or via CMake / CTest
cd build && ctest --output-on-failure
```

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
