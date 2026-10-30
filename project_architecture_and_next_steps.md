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
| **Folder Sync Engine** | `utp sync <src> <dest>` | **Core MVP (Production)** | Phase 6 Delta Change Detection (0-byte skip) + 5 Conflict Policies + Bidirectional |
| **Multi-Destination Fan-Out** | `utp route <src> <d1> <d2>...` | **Core MVP (Production)** | Phase 7 Zero-Disk Fan-Out Streaming + ALL_OR_NOTHING & BEST_EFFORT Policies |
| **REST Daemon (`utpd`)** | REST API (`/api/transfers`) | *Experimental (Post-MVP)* | Phase 5 Marked with `X-UTP-Surface: experimental` |
| **Web Dashboard** | Browser UI (`web/`) | *Experimental (Post-MVP)* | Phase 5 Marked with `[EXPERIMENTAL PREVIEW]` |
| **Python SDK** | `utp-client` (`sdk/python/`) | *Experimental (Post-MVP)* | Phase 5 Beta package targeting experimental daemon API |
| **Extended Clouds** | `s3://`, `r2://`, `b2://`, `azure://`, `dropbox://`, `icloud://`, `webdav://` | *Experimental (Post-MVP)* | Phase 5 Marked experimental pending multi-cloud DoD gates |

---

## 🚀 Phase 6: Advanced Folder Synchronization & Delta Sync (Accomplished)

Delivered in v2.6.0:
1. **Delta Change Detection**: Compares trees by relative path hierarchies, file sizes, modification timestamps (`mtime`), and cryptographic hashes (`hash`). Unchanged files skip with 0 bytes transferred.
2. **Conflict Resolution Matrix**: 5 configurable policies: `FAIL`, `OVERWRITE`, `RENAME`, `SKIP`, `NEWER_WINS`.
3. **Reconciliation Modes**: `UNIDIRECTIONAL` (mirror/push) and `BIDIRECTIONAL` (two-way merge).
4. **Resilience & State Persistence**: SQLite `sync_sessions` and `sync_items` tables in WAL mode with §7 event stream.
5. **CLI Integration**: `utp sync <src> <dest> [--policy <policy>] [-b|--bidirectional] [-n|--dry-run]`.
6. **Automated Test Coverage**: 3 dedicated test suites added in `tests/test_runner.cpp`.

---

## 🚀 Phase 7: Multi-Destination Routing & Fan-Out Streaming (Accomplished)

Delivered in v2.6.0:
1. **Zero-Disk Shared Memory Fan-Out**:
   - Single-source read: bytes are read once from the source endpoint into volatile RAM ring buffers and broadcast in parallel across TLS 1.3 socket streams to $N$ destination cloud targets.
   - Strict memory discipline (§10): No disk staging; buffers are reclaimed as destination workers commit chunks.
2. **Configurable Fault-Tolerance Policies**:
   - `ALL_OR_NOTHING`: If any target fails, all destination upload sessions are aborted (`abort()`) and changes rolled back.
   - `BEST_EFFORT`: Surviving destinations continue streaming to completion and commit even if one target encounters quota or network errors.
3. **Multi-Destination SQLite Persistence**:
   - Dedicated `multi_transfers` and `multi_destinations` tables track multi-target progression, per-destination byte counters, and independent SHA-256 hashes.
   - §7 Event stream: `MULTI_TRANSFER_STARTED`, `MULTI_DEST_CHUNK_COMPLETED`, `MULTI_DEST_COMPLETED`, `MULTI_DEST_FAILED`, `MULTI_TRANSFER_COMPLETED`, `MULTI_TRANSFER_FAILED`.
4. **CLI Integration**:
   - `utp route <src> <d1> <d2> [d3...] [--policy <ALL_OR_NOTHING|BEST_EFFORT>]`
   - Also supported directly via `utp transfer <src> <d1> <d2>...`
   - Visual terminal UI with per-destination verification reporting.
5. **Automated Test Coverage**:
   - 3 dedicated test suites added to `tests/test_runner.cpp`:
     - `test_multi_dest_fanout_suite` (1 source -> 3 destinations simultaneous broadcast + SHA-256 validation)
     - `test_multi_dest_all_or_nothing_policy_suite` (Simulated destination failure -> automatic rollback of peer target)
     - `test_multi_dest_best_effort_policy_suite` (Partial failure -> surviving target commits successfully)
   - Total test suites: **22/22 passing (100% green)**.

---

## Active Status & Next Roadmap: v2.6.0

With Phases 6 & 7 complete, the remaining focus areas for **v2.6.0** are:

### 1. Native TypeScript SDK (Next Up)
- Publish `@utp/client` on npm to accompany `utp-client` on PyPI for Node.js and browser integration.
- TypeScript definitions, event streaming, and native promises.

### 2. Background Transfer Daemon Mode
- `utp daemon start / stop / status` to run the daemon server natively as a system service (Windows Service / systemd unit / launchd).


