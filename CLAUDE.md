# CLAUDE.md — UTP (Universal Cloud Transfer Protocol)

Persistent project context. Claude Code loads this automatically at the start of every session in this repo. Re-read it after `/compact` or a long session — the decisions here override the instinct to "just make it work" in whatever way is fastest for the current task.

## 1. What we're building

UTP is a provider-independent protocol and reference engine for reliably moving files and directories between heterogeneous storage systems (Google Drive, OneDrive, S3, Dropbox, local disk, NAS, etc.), without necessarily routing the full byte volume through the user's machine.

The protocol + engine is the product. CLI, web app, API, and SDKs are clients of that engine, built later — none of them should accumulate logic that belongs in the engine itself.

## 2. What UTP explicitly is NOT (scope discipline)

- Not a replacement for Google Drive / OneDrive / S3 / etc. They remain the storage; UTP is the mobility layer between them.
- Not, yet, an interoperable industry protocol. We're at "experimental specification + reference implementation." A second, independent implementation that interoperates with ours is the bar for dropping "experimental" — don't describe it as more than that anywhere in docs or comments.
- Not Kubernetes, microservices, Redis, Kafka, a fancy dashboard, a mobile app, an AI assistant, blockchain, or P2P networking — at any point before the MVP (§12) is done and proven reliable. If a task seems to require one of these, stop and reconsider the approach rather than reaching for it.

## 3. Non-negotiable design principles

| Principle | What it means in code |
|---|---|
| Provider independence | `core/` never contains a provider name or `if (provider == "google")`. Provider-specific logic lives only in `providers/*`. |
| Resumability | Any operation that can checkpoint, does. A crash mid-transfer must never force a restart from zero. |
| Verifiability | A successful upload call is not a successful transfer. Always verify end to end. |
| Capability negotiation | The planner queries each provider's `ProviderCapabilities` before deciding *how* to transfer. Never assume a capability — check it. |
| Extensibility | Adding a provider means writing one new adapter class. Zero changes to `core/` or `providers/interface/`. If a new provider forces a core change, the interface was wrong — fix the interface, don't special-case the provider. |

## 4. Architecture

**Control plane** — auth, discovery, transfer creation, capability negotiation, scheduling, status, retries, cancellation, metadata, transfer state. Almost no file bytes pass through here.

**Data plane** — the bytes themselves, streamed source → destination through a bounded buffer.

Keep these separated at the module level, not just conceptually: a control-plane component shouldn't hold a byte buffer, and a data-plane component shouldn't make scheduling decisions.

```
USER → CLI / Web / SDK / API → UTP Controller → Transfer Planner
                                                     │
                                    ┌────────────────┴────────────────┐
                              Source Provider                 Destination Provider
                                    │                                  │
                                    └────────────────┬─────────────────┘
                                                      ▼
                                             Transfer Engine
                              (Chunk Manager · Worker Pool · Retry Manager ·
                               Resume Manager · Rate Limiter · Integrity Engine ·
                               State Manager)
```

## 5. Core contracts — change these deliberately, not casually

**StorageProvider interface** (`providers/interface/`):
```cpp
class StorageProvider {
public:
    virtual ProviderCapabilities capabilities() = 0;
    virtual FileInfo stat(Path path) = 0;
    virtual std::vector<FileInfo> list(Path path) = 0;
    virtual ReadStream openRead(Path path) = 0;
    virtual WriteSession openWrite(Path path, uint64_t size) = 0;
    virtual void commit(WriteSession session) = 0;
    virtual void abort(WriteSession session) = 0;
    virtual ~StorageProvider() = default;
};
```
The engine talks to this interface only. `LocalProvider`, `GoogleDriveProvider`, `OneDriveProvider`, `S3Provider`, etc. implement it and nothing provider-specific leaks upward.

**ProviderCapabilities:**
```cpp
struct ProviderCapabilities {
    bool ranged_read;
    bool resumable_upload;
    bool multipart_upload;
    bool server_side_copy;
    bool checksum;
    bool metadata;
    bool directory_support;
    uint64_t max_chunk_size;
    uint32_t max_parallel_requests;
};
```
Providers report these honestly. Verify each value against that provider's current API docs before hardcoding it — never assume.

**Resource identifiers:**
```
local:///home/user/movie.mp4
gdrive://my-drive/projects/model.bin
onedrive://documents/report.pdf
s3://bucket/models/model.bin
```
These are UTP resource identifiers, not necessarily public URLs.

**Transfer state shape:**
```json
{
  "id": "utp_01JXZ92D8H4F6A1",
  "source": { "provider": "google-drive", "path": "/Research/data.zip" },
  "destination": { "provider": "onedrive", "path": "/Backup/data.zip" },
  "size": 107374182400,
  "state": "TRANSFERRING",
  "bytes_transferred": 62411243520,
  "chunk_size": 8388608
}
```

**Transfer state machine:**
```
CREATED → AUTHENTICATING → DISCOVERING → NEGOTIATING → PLANNING
        → TRANSFERRING → VERIFYING → COMMITTING → COMPLETED
```
Side states reachable at the appropriate points: `PAUSED`, `RETRYING`, `REPAIRING`, `CANCELLED`, `FAILED`. Every transition is persisted (§6) before it's acted on — a crash must never leave on-disk state ambiguous about what actually happened.

## 6. Persistence

SQLite for the reference implementation — reliability without infrastructure weight. Core tables: `transfers`, `objects`, `chunks`, `providers`, `sessions`, `events`.

```
transfers(id, source_uri, destination_uri, state, bytes_total, bytes_completed, created_at, updated_at)
chunks(transfer_id, object_id, chunk_index, offset, size, state, checksum, attempts)
```
Postgres is a later server-deployment concern — don't introduce it into the reference engine.

## 7. Events

The engine emits these; CLI/API/Web all consume the same stream instead of polling ad hoc state:
`TRANSFER_CREATED, TRANSFER_STARTED, OBJECT_STARTED, CHUNK_COMPLETED, OBJECT_COMPLETED, TRANSFER_PAUSED, TRANSFER_RESUMED, TRANSFER_VERIFIED, TRANSFER_COMPLETED, TRANSFER_FAILED`

## 8. Error classification

```
ERROR ├── RETRYABLE   → retry with exponential backoff + jitter
      ├── AUTH        → refresh / re-authenticate
      ├── RATE_LIMIT  → provider-aware backoff
      └── FATAL       → fail, surface to user
```

## 9. Tech stack

| Layer | Choice | Why |
|---|---|---|
| Core engine | C++20 | Fine-grained control over memory, networking, and concurrency for large-file streaming |
| Build | CMake | |
| HTTP | libcurl | |
| Hashing | OpenSSL (SHA-256) | |
| Persistence | SQLite | Reliable, zero extra infrastructure |
| Serialization | JSON | Start simple |
| Testing | Catch2 or GoogleTest | |
| CLI | C++ | Same binary as the engine, initially |

Backend/API language and frontend framework are post-MVP decisions — don't pre-build them.

## 10. Memory discipline (hard rule)

Never load a whole file into memory. A 500 GB file is streamed through a small bounded buffer (e.g. 8 MB), reused per chunk. If a fast source (e.g. 500 MB/s) is paired with a slow destination (e.g. 80 MB/s), the source must be backpressured by a bounded queue — buffers do not grow unbounded. This discipline starts at the very first "naive" local→local copy, not once concurrency is added later.

## 11. Repository layout

```
utp/
├── protocol/    # SPEC.md, terminology.md, resource-identifiers.md, transfer-lifecycle.md,
│                  capabilities.md, integrity.md, errors.md, security.md
│                  — fill these in as each subsystem is actually built, not speculatively ahead of it
├── core/        # transfer, planner, scheduler, chunking, verification, recovery,
│                  persistence, events, networking
├── providers/   # interface, local, google-drive, onedrive, s3
├── cli/
├── sdk/         # python, typescript — post-MVP
├── server/      # post-MVP
├── web/         # post-MVP
├── tests/       # unit, integration, conformance, failure
├── benchmarks/
├── examples/
└── docs/
```

## 12. MVP — the only thing that matters right now

```
Local → Local
Google Drive → OneDrive
```
with large files, streaming, resume, retries, progress, integrity, and persisted state. Nothing else — not five providers, not a web UI, not an API — until this is proven reliable. See `PROMPTS.md` for the milestone-by-milestone build order.

**Definition-of-done test for the whole MVP — run it, don't just reason about it:** start a transfer of a multi-GB file, `kill -9` the process partway through, run `utp resume <id>`, and confirm it (a) skips already-completed chunks, (b) completes, (c) source and destination SHA-256 match.

## 13. Before you call anything "done"

- [ ] Unit test exists for new logic; integration test exists for new cross-component behavior.
- [ ] No full-file buffering was introduced.
- [ ] No provider name leaked into `core/`.
- [ ] State transitions are persisted, not just held in memory.
- [ ] Relevant event(s) from §7 are emitted.
- [ ] Nothing from the "Post-MVP" list in `PROMPTS.md` snuck in to get here.
