# PROMPTS.md — UTP Build Sequence

Milestone-by-milestone prompts for implementing UTP, derived from the blueprint's roadmap. Assumes `CLAUDE.md` is already in the repo root.

**How to use this:** work through milestones in order, one per session (fresh session or `/compact` first — don't chain milestones in one giant context). Copy the fenced prompt block for the milestone you're on and send it as-is. Don't move to the next milestone until its "Done when" bar is actually met, not just plausible — several of these specify a real test to run, not just code to write.

---

## M0 — Repository Bootstrap & Core Types
*Roadmap steps 1–2*

```
Implement Milestone 0 of UTP. Full context is in CLAUDE.md — read it first.

Goal: bootstrap the repo skeleton and core value types. No transfer logic yet.

Do:
- Create the repository tree from CLAUDE.md §11 (protocol/, core/, providers/, cli/,
  sdk/, server/, web/, tests/, benchmarks/, examples/, docs/). Empty dirs get a
  .gitkeep; don't invent files nothing needs yet.
- Root CMakeLists.txt + core/CMakeLists.txt, targeting C++20, with Catch2 or
  GoogleTest wired into tests/unit/.
- Core value types only, no logic: FileInfo, ProviderCapabilities, ChunkInfo, a
  TransferState enum matching CLAUDE.md §5's state machine, and a Transfer struct
  matching CLAUDE.md §5's JSON shape.
- One placeholder unit test proving the build+test harness actually runs.

Do NOT: write networking code, a provider implementation, or a URI parser. Do NOT
add SQLite, libcurl, or OpenSSL as dependencies yet.

Done when: `cmake --build .` succeeds and the test runner reports the placeholder
test passing, on a clean checkout.
```

---

## M1 — URI Parser & Provider Contract
*Roadmap steps 3–4*

```
Implement Milestone 1 of UTP (CLAUDE.md has full context; M0 must already be merged).

Goal: parse UTP resource identifiers, and define the provider contract —
interface only, no concrete provider yet.

Do:
- URI parser turning local:///path, gdrive://drive/path, onedrive://path,
  s3://bucket/path into a structured Resource{scheme, host_or_bucket, path}.
  Unit-test the edge cases: missing scheme, empty path, trailing slash, unusual
  characters, malformed input — assert it rejects garbage rather than silently
  misparsing it.
- StorageProvider abstract interface in providers/interface/, exactly as
  specified in CLAUDE.md §5.
- A MockProvider in tests/ — in-memory, implements the full interface — so later
  engine tests don't need a real filesystem or network call.

Do NOT: implement LocalProvider (that's M2). Do not touch any cloud API.

Done when: URI parser tests cover the edge cases above and pass; MockProvider
compiles against StorageProvider and a trivial test exercises each of its methods.
```

---

## M2 — Local Provider & First End-to-End Transfer
*Roadmap steps 5–6*

```
Implement Milestone 2 of UTP (M0, M1 merged; CLAUDE.md for context).

Goal: a real, working LocalProvider, and the simplest possible engine that can
do a single-shot local→local copy. No chunking, resume, retries, or integrity
checking yet — those are separate milestones on purpose. The point here is
proving the provider/engine boundary works end to end, honestly.

Do:
- LocalProvider implementing StorageProvider over the filesystem.
- A minimal transfer engine that copies via a bounded buffer (e.g. 8MB), reused
  across the copy loop. This discipline starts now, not once "we get to
  backpressure." No read() of a whole file into memory, even at this naive stage.
- CLI stub: utp transfer local://<src> local://<dst>

Do NOT: chunk the file, persist state, retry on failure (log-and-exit is fine),
or implement any other provider.

Done when: `utp transfer local:///tmp/a.bin local:///tmp/b.bin` on a multi-GB
test file produces a byte-identical output file, and peak memory usage doesn't
scale with file size (spot-check with `/usr/bin/time -v` or equivalent).
```

---

## M3 — Chunk Planner & Persistent State
*Roadmap steps 7–8*

```
Implement Milestone 3 of UTP (M0–M2 merged).

Goal: introduce chunking, and make transfer state survive a process restart.
Resume logic itself is the next milestone — this one is purely "state is
faithfully persisted."

Do:
- Chunk planner: fixed chunk size to start (8MB default), producing a
  TransferPlan — a list of ChunkInfo{offset, size, state}.
- SQLite schema + migrations for transfers and chunks (CLAUDE.md §6). Engine
  writes chunk-state transitions to the DB as it processes them, sequentially —
  still single-threaded, no concurrency yet (that's M5).

Do NOT: implement retry logic, concurrency, or resume-from-crash. Just persist
honestly.

Done when: after a completed transfer, the SQLite DB shows every chunk COMPLETE
and the transfer COMPLETED. A transfer killed mid-way leaves a DB row showing
genuinely partial chunk completion — inspect it manually to confirm, even
though nothing consumes it yet.
```

---

## M4 — Resume & Integrity (the project's actual proof point)
*Roadmap steps 9–10*

```
Implement Milestone 4 of UTP (M0–M3 merged). This is the most important
milestone in the whole MVP — don't rush it and don't mark it done on a
partial pass.

Goal: wire up real resume-from-persisted-state, and SHA-256 end-to-end
verification.

Do:
- `utp resume <transfer_id>`: reads SQLite state, skips COMPLETE chunks,
  continues from the first INCOMPLETE/PENDING chunk.
- Incremental SHA-256 on both the source-read and destination-write paths;
  compare at the VERIFYING/COMMITTING transition.
- CLI status output: progress bar, transferred/remaining, state.

Do NOT: add retry/backoff or concurrency (M5), or any cloud provider (M6+).
Stays local→local, single-threaded — but must be genuinely, provably resumable.

Done when — run this exact test, don't just reason about it: start a transfer
of a 5–20GB local test file. Partway through, `kill -9` the process. Run
`utp resume <transfer_id>`. Confirm: (a) already-complete chunks are not
re-transferred (check timing/logs, not just the end result), (b) the transfer
completes, (c) source and destination SHA-256 match exactly. This is the
project's core promise — treat it as a hard gate before moving on.
```

---

## M5 — Retry, Concurrency, Backpressure
*Roadmap steps 11–13*

```
Implement Milestone 5 of UTP (M0–M4 merged).

Goal: make the engine resilient and fast — classified retry, a bounded worker
pool, and real backpressure between read and write paths.

Do:
- Retry policy per CLAUDE.md §8's error classes (RETRYABLE/AUTH/RATE_LIMIT/
  FATAL), exponential backoff + jitter.
- Bounded worker pool transferring multiple chunks in parallel, parallelism
  configurable.
- Bounded producer/consumer queue between read and write paths so a fast
  source can't outrun a slow destination.
- A fault-injecting fake provider for tests (fails N% of chunk operations with
  retryable errors; can be throttled to simulate a slow destination).

Do NOT: build provider-specific rate-limit handling yet — there's still only
LocalProvider. Build the mechanism generically and prove it against the fake
provider, not a real cloud API.

Done when: the fault-injection test still completes correctly with retryable
failures present; an artificially-slow-destination test shows bounded memory
instead of unbounded buffering (measure it, don't assume it).
```

---

## M6 — Google Drive Adapter + OAuth
*Roadmap steps 14–15*

```
Implement Milestone 6 of UTP (M0–M5 merged). First real cloud provider — this
milestone is also a test of whether the abstraction in CLAUDE.md §5 actually
holds up against a real API.

Goal: GoogleDriveProvider against the real Drive API, plus OAuth.

Do:
- OAuth2 (authorization code + refresh flow); tokens encrypted at rest, never
  logged, scoped as narrowly as the Drive API allows.
- GoogleDriveProvider implementing StorageProvider, reporting
  ProviderCapabilities truthfully — verify what Drive's API actually supports
  (resumable upload, ranged read, server-side copy, checksums, etc.) against
  current API documentation rather than assuming.

Do NOT: modify core/ or providers/interface/ to accommodate a Drive-specific
quirk. If it seems necessary, stop and flag it as a design question rather
than quietly special-casing it. Do NOT start OneDrive yet.

Done when: `utp transfer gdrive://<path> local:///tmp/out` and the reverse
direction both work; the kill-9-then-resume-then-verify test from M4 passes
specifically against Google Drive; the provider passes the conformance
checklist (auth, list, stat, ranged read, create, write, resume, commit,
checksum, cancellation, error mapping).
```

---

## M7 — OneDrive Adapter + OAuth
*Roadmap steps 16–17*

```
Implement Milestone 7 of UTP (M0–M6 merged). Same shape as M6, targeting
Microsoft Graph / OneDrive.

Do: OAuth2 against Microsoft's identity platform; OneDriveProvider
implementing StorageProvider with honestly-reported capabilities (verify
against current Graph API docs).

Do NOT: special-case anything in core/. If M6 required a core change for
Google, apply that same fix here rather than adding a second workaround —
needing the same fix twice for two different providers means the interface
itself needs revising, not that both providers are exceptions.

Done when: local↔OneDrive works both directions with resume+integrity, and
OneDrive passes the same conformance checklist Google did in M6.
```

---

## M8 — Cross-Cloud Transfer & Failure Testing (true MVP finish line)
*Roadmap steps 18–19*

```
Implement Milestone 8 of UTP (M0–M7 merged). Per CLAUDE.md §12, this
milestone is the actual definition of "the MVP is done."

Goal: Google Drive → OneDrive direct, and the full failure-injection suite.

Do:
- Relay-mode transfer (CLAUDE.md's Strategy B): stream chunk-by-chunk between
  the two cloud APIs through a bounded buffer — no full-file staging to local
  disk or memory. (Neither provider is likely to support true server-side
  cross-vendor copy; confirm rather than assume, but relay is the expected path.)
- Automate the full failure-injection suite: kill mid-transfer, network
  disconnect, expired OAuth token, simulated HTTP 429, simulated HTTP 503,
  corrupted chunk, destination out of space, source file vanishes mid-transfer,
  destination file already exists (test each conflict policy: FAIL/OVERWRITE/
  RENAME/SKIP).

Do NOT: build automatic strategy selection (native vs. relay vs. client-side)
if relay is the only strategy actually implemented — a "chooser" with one
option is theater. Add real selection logic honestly once a second strategy
exists.

Done when: `utp transfer gdrive://Research/data.zip onedrive://Backup/data.zip`
completes reliably on a real multi-GB file, and every test in the
failure-injection suite passes as an automated test, not a manual spot check.
Nothing past this point — API, web UI, SDKs, more providers — starts until
this is genuinely solid.
```

---

## M9 — CLI Polish
*Roadmap step 20*

```
Implement Milestone 9 of UTP (M0–M8 merged).

Goal: make the CLI usable by someone who hasn't read the source.

Do: `utp auth google` / `utp auth microsoft`, `utp providers`, `utp ls <uri>`,
the status/progress display (progress bar, speed, transferred/remaining,
retries, workers, state), `utp pause` / `utp resume` / `utp cancel` wired to
the PAUSED/CANCELLED states.

Do NOT: start on the API server, web UI, or SDKs.

Done when: someone unfamiliar with the codebase can authenticate, list a
remote path, start a transfer, and check its status without reading source code.
```

---

## Post-MVP — do not expand these into prompts until M0–M9 are solid

The blueprint is explicit that these come after, not alongside, the MVP:

- **API** (`POST /v1/transfers`, `GET /v1/transfers/:id`, etc.)
- **Web application** (new-transfer UI, dashboard)
- **SDKs** (Python, TypeScript)
- **More providers** (S3, Dropbox, Azure Blob, iCloud)
- **Sync, multi-destination routing, rules-based routing, deduplication, delta transfer**
- **Privacy-focused mode** (explicit local/relay/native strategy selection UI)

When you're ready for one of these, write it as a milestone prompt in the same
format — goal / do / do-not / done-when — rather than starting from a vague
feature request. Each of these, expanded prematurely, is exactly the kind of
scope creep CLAUDE.md §2 warns against.
