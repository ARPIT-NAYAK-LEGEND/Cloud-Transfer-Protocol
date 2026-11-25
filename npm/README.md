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

**Zero-Disk Cloud-to-Cloud Transfer, Direct Web Ingestion, Media Streaming & Sync Engine**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Official Portal](https://img.shields.io/badge/Official%20Site-ctp.arpitnayak.tech-FF5B1F.svg)](https://ctp.arpitnayak.tech)
[![Privacy & Terms](https://img.shields.io/badge/Legal-Privacy%20%26%20Terms-22c55e.svg)](https://ctp.arpitnayak.tech/privacy)
[![Release](https://img.shields.io/badge/Release-v2.6.1--pro-cyan.svg)](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/releases)
[![Platforms](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-emerald.svg)](#installation)

</div>

---

## What is UTP?

**UTP (Universal Transfer Protocol)** is a high-performance cloud migration, direct download, synchronization, and on-demand media streaming engine written in C++20. Instead of downloading files to disk before re-uploading, UTP streams bytes **directly through dynamically bounded RAM ring buffers over TLS 1.3** with zero local disk staging.

### Key Capabilities

- **Cloud-to-Cloud Migration**: Stream files directly between Google Drive, Microsoft OneDrive, S3, Cloudflare R2, Backblaze B2, Azure Blob, Dropbox, Apple iCloud, and WebDAV/Nextcloud.
- **Direct Web URL Ingestion (`utp ingest` / `https://`)**: Stream files, ISOs, videos, and seedbox downloads straight from any HTTP/HTTPS URL into cloud storage or local disk — with automatic `HEAD` / `Range: bytes=0-0` size probing and `Content-Disposition` filename extraction.
- **Direct Cloud-to-Local Download**: Download single files or recursive folders directly from any connected cloud provider to local storage (`file://`) via TUI, CLI, or daemon `?download=true`.
- **Zero-Disk Media Streaming (`utp stream` / `utp play`)**: Stream cloud video, audio, or PDFs directly to VLC, MPV, or web browsers over an instant local HTTP/1.1 `206 Partial Content` range server (`http://127.0.0.1:8080/stream`) with full seek/scrub support and 0 bytes written to disk.
- **Hardware-Adaptive Chunking**: Automatically probes physical system RAM and adapts chunk sizes to available memory and provider constraints.
- **Interactive Visual TUI**: Master operations menu, multi-account browser OAuth, breadcrumb directory navigation, folder creation, direct web URL ingestion, and live throughput HUD (`utp`).
- **Delta Folder Sync Engine**: Unidirectional mirror or bidirectional two-way sync with 0-byte unchanged skips and 5 conflict policies (`OVERWRITE`, `FAIL`, `RENAME`, `SKIP`, `NEWER_WINS`).
- **Multi-Destination Fan-Out & Smart Routing**: Broadcast one source read pass to multiple destinations concurrently, or route files dynamically using `ext`, `size`, `path`, and `name` rules.
- **Resumable ACID Ledger & Background Daemon**: Crash-resilient SQLite WAL ledger (`utp.db`), plus `utpd` REST daemon (`utp daemon start|stop|status`) with Bearer auth and `/api/stream`.

---

## Installation

### Instant Run with `npx` (No global setup)
```bash
npx utp-engine
```

### Global Install
```bash
npm install -g utp-engine
```

Exposes both the CLI (`utp`, `utp-engine`) and REST daemon (`utpd`, `utp-server`).

### Alternative Installers
```bash
# Universal Installer (Linux & macOS)
curl -fsSL https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/install.sh | bash

# Homebrew (macOS & Linux)
brew install ARPIT-NAYAK-LEGEND/utp/utp

# Scoop (Windows)
scoop install https://raw.githubusercontent.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/PROJECTS/packaging/scoop/utp.json
```

---

## Usage

### 1. Interactive TUI (Operations Hub)
```bash
utp
```
Opens the interactive hub where you can:
- **Transfer / Direct Web Ingestion**: Move files cloud-to-cloud, stream a direct HTTPS URL (`https://`) straight into a cloud drive, or download cloud files to local storage (`file://`).
- **Stream & Play directly (Zero-Disk)**: Select any cloud file -> pick **"Stream & Play directly (Zero-Disk)"** (or launch from main menu) to watch instantly in VLC, MPV, or your browser. Press `q`, `Enter`, `Esc`, or `Ctrl+C` to stop streaming and return cleanly to the main menu.
- **Ledger History**: Inspect recent transfers, speeds, and completion state.

### 2. Direct Web URL Ingestion & Cloud Download
```bash
# Stream an HTTPS URL straight into Google Drive or OneDrive (zero disk)
utp ingest https://example.com/archive.tar.gz gdrive://Backups/

# Transfer a direct web URL to S3 or local disk
utp transfer https://example.com/distro.iso s3://isos/distro.iso

# Download a cloud file directly to local disk
utp transfer gdrive://1BxiMVs0XRA5nF... file:///C:/Downloads/report.pdf --id dl-report-1
```

### 3. Zero-Disk Cloud Media Streaming
```bash
# Interactive cloud picker -> launches stream server on http://127.0.0.1:8080/stream
utp stream

# Stream and auto-open default player / browser
utp play

# Stream a specific cloud URI on a custom port
utp stream gdrive://1BxiMVs0XRA5nF... --port 9090 --open
```

Supported media MIME types include `.mp4`, `.m4v`, `.mkv`, `.webm`, `.avi`, `.mov`, `.wmv`, `.flv`, `.ts`, `.mp3`, `.flac`, `.wav`, `.m4a`, `.aac`, `.ogg`, and `.pdf` with full HTTP `206` byte-range seeking.

### 4. Direct Cloud-to-Cloud Transfer
```bash
# Google Drive to Microsoft OneDrive
utp transfer gdrive://1BxiMVs0XRA5nF... onedrive://Backups/data.zip

# Cloudflare R2 to S3 with 20 MB/s bandwidth limit
utp transfer r2://assets/video.mp4 s3://archive/video.mp4 --bandwidth 20971520
```

### 5. Delta Folder Synchronization
```bash
# Mirror local directory to Google Drive
utp sync file://my_project gdrive://backups/my_project

# Bidirectional sync with dry-run preview
utp sync file://docs onedrive://Documents -b -n

# Conflict resolution: OVERWRITE, FAIL, RENAME, SKIP, NEWER_WINS
utp sync file://data s3://bucket/data --policy newer_wins
```

### 6. Multi-Destination Fan-Out & Smart Routing
```bash
# Fan-out broadcast (single RAM read -> multiple destinations)
utp route s3://bucket/dataset.tar.gz gdrive://backups/dataset.tar.gz onedrive://archives/dataset.tar.gz --policy best_effort

# Rules-based routing
utp route ./data \
  --rule "ext=mp4,mkv:s3://media-vault/" \
  --rule "size>100M:b2://cold-archive/" \
  --default "file://fallback_dir/"
```

### 7. Transfer Lifecycle & Background Daemon
```bash
utp status <id>      # Inspect state, chunks & event log
utp resume <id>      # Resume paused or interrupted transfer
utp pause <id>       # Pause at next chunk boundary
utp cancel <id>      # Abort transfer & clean up partial upload
utp list             # Recent transfers in utp.db

# Background REST daemon (`utpd`)
utp daemon start --port 8080
utp daemon status
utp daemon stop
```

Daemon `/api/stream` endpoint (Bearer token / `X-API-Key` / `?token=` authenticated):
- `GET /api/stream?uri=gdrive://...&token=<token>` — HTTP 206 stream playback
- `GET /api/stream?uri=gdrive://...&download=true&name=video.mp4&token=<token>` — Direct download attachment

---

## CLI Quick Reference

| Command | Arguments | Flags | Description |
| :--- | :--- | :--- | :--- |
| `utp` / `utp menu` | *(none)* | *(none)* | Interactive operations hub |
| `utp transfer` | *(none)* | *(none)* | Interactive TUI file explorer & transfer |
| `utp transfer` | `<src> <dest>` | `--bandwidth`, `--id`, `--verbose` | Direct cloud / web / local streaming transfer |
| `utp ingest` | `<url> <dest>` | `--bandwidth`, `--id` | Direct HTTP/HTTPS URL ingestion to cloud or local |
| `utp stream` | `[cloud-uri]` | `--port`, `--open` (`-o`) | Zero-disk HTTP 206 media streaming server |
| `utp play` | `[cloud-uri]` | `--port` | Stream cloud media and auto-open player |
| `utp sync` | `<src> <dest>` | `-b`, `-n`, `--policy` | Recursive delta folder sync |
| `utp route` | `<src> <d1> <d2>...` | `--policy`, `--bandwidth` | Multi-destination fan-out broadcast |
| `utp route` | `<src>` | `--rule`, `--default` | Rules-based smart routing engine |
| `utp status` | `<id>` | *(none)* | Inspect state, chunks, and events |
| `utp resume` | `<id>` | `--bandwidth` | Resume interrupted transfer |
| `utp pause` | `<id>` | *(none)* | Pause transfer between chunks |
| `utp cancel` | `<id>` | *(none)* | Abort transfer and discard partial state |
| `utp list` | *(none)* | *(none)* | Show recent ledger entries |
| `utp daemon` | `start\|stop\|status` | `--port` | Manage `utpd` REST API server |

---

## Supported Providers

| Provider | URI Scheme | Status |
| :--- | :--- | :--- |
| Local Storage | `file://<path>` | Core Production |
| Google Drive | `gdrive://<id>` | Core Production |
| Microsoft OneDrive | `onedrive://<path>` | Core Production |
| Direct Web URLs | `https://<url>`, `http://<url>` | Experimental (Read/Ingest) |
| Dropbox | `dropbox://<path>` | Experimental |
| Apple iCloud | `icloud://<path>` | Experimental |
| Amazon S3 | `s3://<bucket>/<key>` | Experimental |
| Cloudflare R2 | `r2://<bucket>/<key>` | Experimental |
| Backblaze B2 | `b2://<bucket>/<key>` | Experimental |
| Azure Blob | `azure://<container>/<blob>` | Experimental |
| WebDAV / Nextcloud | `webdav://<path>` | Experimental |

---

## Security & Privacy

- **TLS 1.3 In-Transit**: Direct encrypted TLS sockets with strict certificate verification (`CURLOPT_SSL_VERIFYPEER = 1`, `CURLOPT_SSL_VERIFYHOST = 2`).
- **Zero Intermediary Servers**: Bytes stream strictly in volatile RAM between your client and cloud endpoints.
- **In-Flight Cryptographic Integrity**: Dual SHA-256 + MD5 verification against provider hashes without cloud read-back.
- **Local Credential Hygiene**: OAuth tokens remain solely on your local machine in `.utp_*_token.json`. No telemetry or external logging.
- **No Shell Injection**: npm launcher uses `child_process.spawn(..., { shell: false })` and `execFileSync` with HTTPS-only release fetches.

---

## Links

- **Homepage**: [ctp.arpitnayak.tech](https://ctp.arpitnayak.tech)
- **GitHub**: [ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol)
- **Privacy Policy**: [PRIVACY.md](PRIVACY.md) · [ctp.arpitnayak.tech/privacy](https://ctp.arpitnayak.tech/privacy)
- **Security Policy**: [SECURITY.md](SECURITY.md)
- **TypeScript SDK**: [`@utp/client`](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/tree/PROJECTS/sdk/typescript)

---

## License

[MIT](LICENSE) © Arpit Nayak
