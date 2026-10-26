# UTP Error Handling & Classification Specification

Failures across cloud networks are unavoidable. UTP categorizes all errors into a formal taxonomy to determine automatic recovery behavior and guarantees non-destructive failure states.

---

## 1. Error Classification Tree

```
ERROR
├── RETRYABLE   → Transient network drops, connection resets, 500/502/503/504 errors
├── RATE_LIMIT  → HTTP 429 Too Many Requests, User Rate Limit Exceeded
├── AUTH        → Expired OAuth token, refresh token invalidated, 401 Unauthorized
└── FATAL       → Non-existent object (404), Insufficient permissions (403), Malformed path
```

---

## 2. Retry Policy & Exponential Backoff

For `RETRYABLE` and `RATE_LIMIT` errors, the transfer engine implements exponential backoff with full jitter to avoid thundering-herd issues on cloud API endpoints:

```
delay = min(base_delay * 2^attempt, max_delay) + random_jitter()
```

- **Default base delay**: 500 ms
- **Max delay**: 30,000 ms (30 seconds)
- **Max retry attempts per chunk**: 5 attempts
- **Full jitter range**: `[0, delay * 0.25]`

---

## 3. Automatic Token Refresh (AUTH Errors)

When encountering an `AUTH` error (HTTP 401):
1. The engine pauses chunk dispatch.
2. The provider adapter triggers an asynchronous OAuth2 refresh token exchange against the provider's token endpoint (`/oauth2/v2.0/token`, `/token`, etc.).
3. The new access token is written to the secure local token file (`.utp_*_token.json`).
4. In-flight failed requests are seamlessly replayed with the new bearer token without user interruption.

---

## 4. State Durability & Non-Destructive Failure

If chunk retries are exhausted or a unrecoverable error occurs:
1. The chunk state in SQLite is transitioned to `FAILED`, and the chunk attempt counter is recorded via `StateManager::record_chunk_attempt()`.
2. The overall transfer state is transitioned to `FAILED`.
3. The exact error string and HTTP status code are persisted into the database ledger via `StateManager::record_transfer_error(transfer_id, error_message, http_code)`.
4. **Non-Destructive Invariant**: The engine deliberately does **NOT** call `dest_->abort()` on failure. All completed chunks, multipart session tokens, and local `.utp.tmp` buffer states remain intact in the ledger and file system.
5. Users can resume with `utp resume <id>` once the underlying condition (e.g., network reconnect or permissions fix) is resolved. The resume operation skips all chunks already marked `COMPLETE`.
