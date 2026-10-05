# Cloud Transfer Protocol (UTP) - Architecture & Next Steps Plan

Based on a review of the repository's current state and the project blueprint (`CLAUDE.md` and `PROMPTS.md`), the project has successfully completed **Milestones 0 through 3**.

You currently have:
- Core provider interfaces and basic URI parsing (M0, M1).
- A functional `LocalProvider` and single-shot transfer logic via a bounded buffer (M2).
- The `ChunkPlanner` and a persistent `StateManager` using SQLite to track chunk completion (M3).

---

## Next Immediate Step: Milestone 4 (Resume & Integrity)
This is the most critical milestone for the MVP. It proves the core promise of the engine: that a crash does not mean starting over from zero, and that copied bytes are 100% correct.

### 1. Resume Functionality
**Goal:** Implement `utp resume <transfer_id>` that reads persisted state from SQLite and skips already-completed chunks.

**Architectural Updates:**
- **Engine `resume` method:** Add a new public method `void Engine::resume(const std::string& transfer_id)` in `core/src/engine.cpp`.
- **State Hydration:** 
  - Query the `StateManager` to fetch the `Transfer` object and its `ChunkInfo` plan from SQLite.
  - Iterate through the chunks. If a chunk is marked `COMPLETE`, skip it.
  - If a chunk is `PENDING`, `IN_PROGRESS`, or `FAILED`, attempt to process it. (Note: A chunk marked `IN_PROGRESS` during a crash should be treated as incomplete and re-run).
- **CLI Wiring:** Update `cli/main.cpp` to parse `utp resume <id>` and invoke the new engine method.

### 2. SHA-256 Integrity Verification
**Goal:** Ensure the source and destination contents match exactly without reading the file into memory twice.

**Architectural Updates:**
- **Hashing Dependency:** Integrate OpenSSL (`<openssl/sha.h>`) into CMake.
- **Incremental Hashing (Read Path):** As you read from the source into your bounded buffer, update an ongoing SHA-256 hash context for the source file.
- **Incremental Hashing (Write Path):** If the destination provider supported it natively, it would return a hash. However, for a generic `LocalProvider` MVP, we should hash the buffer *before* we issue the write command. 
- **End-of-Transfer Verification:** At the end of the transfer loop, finalize both hashes. Compare them.
  - If they match, transition to `COMMITTING` -> `COMPLETED`.
  - If they mismatch, transition to `FAILED` (or `REPAIRING` later).

> [!CAUTION]
> **Memory Discipline Hard Rule:** Do NOT hash the file by loading it all into memory. The SHA-256 contexts must be updated iteratively over the existing bounded buffer (`EVP_DigestUpdate`).

---

## Future Roadmap & Architecture (Milestones 5 - 8)

### M5: Retry, Concurrency, Backpressure
Once resume and integrity are proven, the engine needs to go fast.
- **Worker Pool:** Introduce a thread pool in `Engine`. Instead of a sequential `for` loop over chunks, dispatch chunk tasks to a bounded number of workers.
- **Backpressure:** A fast source (e.g., NVMe) must not outpace a slow destination (e.g., HDD). Introduce a bounded thread-safe producer-consumer queue for the chunk buffers. 
- **Retry Logic:** Handle transient failures using exponential backoff and jitter based on error classifications (`RETRYABLE`, `RATE_LIMIT`, `AUTH`).

### M6 & M7: Google Drive and OneDrive Adapters
- **OAuth2 Integration:** Implement authorization code flows and token refresh mechanisms. Tokens must be stored securely and scoped narrowly.
- **Adapter Implementations:** Implement `StorageProvider` for `GoogleDriveProvider` and `OneDriveProvider`. 
- **Capability Negotiation:** Dynamically check if the provider supports resumable uploads, server-side copy, or metadata, relying on true API capabilities (updated in `ProviderCapabilities`).

### M8: Cross-Cloud Transfer (MVP Finish Line)
- **Relay Strategy:** Route chunks from Google Drive directly to OneDrive through your engine's bounded memory buffers. 
- **Failure Suite:** Automate chaotic failure testing (simulated HTTP 429/503s, mid-transfer disconnects, expired tokens) to guarantee robustness.

---

## Non-Negotiable Development Principles
As you continue building, ensure these rules from `CLAUDE.md` are strictly adhered to:
1. **Provider Independence:** `core/` files like `engine.cpp` must NEVER contain string checks like `if (provider == "google")`.
2. **Buffer Limits:** Never load entire files into memory. All buffers must have a strict upper bound limit (e.g., 8MB chunks).
3. **No Database Creep:** Do not introduce Postgres, Redis, or Kafka for the reference implementation. Stick to SQLite.
4. **No Premature Post-MVP Features:** Do not build the Web UI, API Server, or Python/TS SDKs until Milestone 8 passes fully. 

> [!TIP]
> **Actionable Next Step:** I am ready to implement the **`Engine::resume()` method and SHA-256 integrity hashing (Milestone 4)**. Would you like me to begin modifying `engine.cpp` and `CMakeLists.txt` for this?
