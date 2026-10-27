# Universal Cloud Transfer Protocol (UTP) - Architecture & Status

## Executive Summary: v2.5.0 Release Accomplished

The Universal Cloud Transfer Protocol (UTP) reference engine and client ecosystem have successfully completed all core milestones (**Milestones 0 through 9**) and shipped release **v2.5.0**.

### Core Architecture & Capabilities
1. **Zero-Disk Streaming Pipeline**:
   - Transfers stream strictly through volatile RAM ring buffers with zero local disk staging.
   - Dynamic adaptive chunk planning scales chunk sizes from 2.5 MB up to 40 MB based on payload size and provider limits.
2. **Server-Side Zero-Bandwidth Direct Copy**:
   - Intra-cloud transfers (e.g. Google Drive ➔ Google Drive, Dropbox ➔ Dropbox) utilize native cloud copy APIs and cross-account sharing.
   - Large multi-gigabyte transfers complete in seconds without downloading or uploading client bytes.
3. **Resilience & State Persistence (`utp.db`)**:
   - SQLite WAL-mode state manager tracks transfer lifecycles and chunk progression.
   - Resumable transfers skip already-completed chunks via `utp resume <id>`.
   - Automatic exponential backoff with jitter and 25% transient network failure auto-recovery.
4. **End-to-End Cryptographic Verification**:
   - Incremental SHA-256 calculation across read and write paths verified before final commit.
5. **Universal 10-Cloud Ecosystem**:
   - Google Drive, Microsoft OneDrive, Dropbox, Apple iCloud, Amazon S3, Cloudflare R2, Backblaze B2, Microsoft Azure Blob Storage, WebDAV / Nextcloud, and Local Filesystem.
   - Multi-account alias syntax (`gdrive:personal://...`, `onedrive:work://...`).
6. **Multi-Interface Support**:
   - **Interactive TUI**: Arrow-key terminal file browser with pagination and batch selection.
   - **CLI Engine**: Single-shot command line transfers with live single-line status reporting.
   - **Daemon API Server (`utpd`)**: RESTful HTTP daemon (`/api/transfers`) supporting all 10 providers.
   - **Web UI**: Glassmorphic real-time dashboard with live transfer tracking and percentage meters.
   - **Python SDK (`utp-client`)**: Programmatic client library for automation.
   - **Cross-Platform Distribution**: Automated binary delivery via `npx utp-engine`, Homebrew, Scoop, and Arch Linux Pacman.

---

## ⚖️ Surface Maturity & Classification Matrix (Phases 1–5 Validated)

In accordance with strict MVP discipline (§12 of `CLAUDE.md`), UTP surfaces are explicitly separated into verified **Core Production MVP** and **Experimental Post-MVP Extensions**:

| Surface | Scope / Component | Maturity Status | Verification Gate |
| :--- | :--- | :--- | :--- |
| **Local Storage** | `file://` provider | **Core MVP (Production)** | Phase 4 Automated DoD `kill -9` → resume → SHA-256 Verified |
| **Google Drive** | `gdrive://` provider | **Core MVP (Production)** | Phase 1 Forced Token Expiry + >100-item recursive upload |
| **OneDrive** | `onedrive://` provider | **Core MVP (Production)** | Phase 1 Large Session Upload + Resume continuation |
| **CLI / TUI Engine** | `utp transfer`, `resume`, `pause`, `cancel` | **Core MVP (Production)** | Phase 2 Lifecycle Events + Persistent Control Signals |
| **State Persistence** | SQLite WAL ledger (`utp.db`) | **Core MVP (Production)** | Phase 2 Upsert semantics + SHA-256 Chunk Verification |
| **Security Hygiene** | Secret scanning & token isolation | **Core MVP (Production)** | Phase 3 Zero client secret leakage (`rg "g_sec\|o_sec"` = 0) |
| **CI / CD Pipeline** | GitHub Actions & Docker build | **Core MVP (Production)** | Phase 4 16-suite test suite + automated DoD on every PR |
| **REST Daemon (`utpd`)** | REST API (`/api/transfers`) | *Experimental (Post-MVP)* | Phase 5 Marked with `X-UTP-Surface: experimental` |
| **Web Dashboard** | Browser UI (`web/`) | *Experimental (Post-MVP)* | Phase 5 Marked with `[EXPERIMENTAL PREVIEW]` |
| **Python SDK** | `utp-client` (`sdk/python/`) | *Experimental (Post-MVP)* | Phase 5 Beta package targeting experimental daemon API |
| **Extended Clouds** | `s3://`, `r2://`, `b2://`, `azure://`, `dropbox://`, `icloud://`, `webdav://` | *Experimental (Post-MVP)* | Phase 5 Marked experimental pending multi-cloud DoD gates |

---

## Active Status & Next Roadmap: v2.6.0

With the foundational protocol, providers, and multi-platform packaging solidly in place, the immediate focus areas for **v2.6.0** are:

### 1. Advanced Folder Synchronization & Delta Sync
- **Bidirectional Sync**: Implement rsync-style delta algorithms to synchronize directories between disparate clouds without re-transferring unchanged files.
- **Conflict Resolution Matrix**: User-configurable policies (`FAIL`, `OVERWRITE`, `RENAME`, `SKIP`, `NEWER_WINS`).

### 2. Multi-Destination Routing
- Stream a single source file to multiple cloud destinations simultaneously via shared memory fan-out buffers (e.g., S3 ➔ Google Drive + OneDrive in a single pass).

### 3. Native TypeScript SDK
- Publish `@utp/client` on npm to accompany `utp-client` on PyPI for Node.js and browser integration.

### 4. Background Transfer Daemon Mode
- `utp daemon start / stop / status` to run the daemon server natively as a system service (Windows Service / systemd unit / launchd).
