# UTP Security & Credential Hygiene Specification

The Universal Cloud Transfer Protocol is designed with zero-trust and client-side privacy as core tenets.

---

## 1. Zero-Disk Memory Footprint (Privacy Guarantee)

UTP streams bytes directly from cloud-to-cloud through volatile RAM ring buffers.
- **No Temporary Staging**: Transferred data is never cached, buffered, or staged to local hard drives or SSDs.
- **Transient Memory Scrubber**: Buffer memory is overwritten and freed immediately after each chunk commit.
- **Sensitive Payload Immunity**: Moving sensitive financial records, private photos, or trade secrets leaves no recoverable trace on the host file system.

---

## 2. In-Transit Encryption

- All provider adapters communicate strictly via HTTPS with TLS 1.3 or TLS 1.2.
- Mutual verification of remote server certificates using platform root certificate bundles.
- Perfect Forward Secrecy (PFS) cipher suites prioritized for all data plane sockets.

---

## 3. Credential Hygiene & Zero Hardcoded Secrets (Phase 3)

Cloud tokens and API secrets are sensitive credentials:
- **No Hardcoded Secrets**: Client secrets are never embedded, compiled into binaries, or XOR-masked in source files. Hardcoded secret arrays have been eliminated entirely.
- **Environment & Keyring Integration**: OAuth client secrets must be supplied via environment variables (`GDRIVE_CLIENT_SECRET`, `ONEDRIVE_CLIENT_SECRET`, `AWS_SECRET_ACCESS_KEY`, etc.) or secure local configuration.
- **Local Isolation**: Tokens are stored exclusively in user-scoped files (`.utp_*_token.json`) located in the application directory.
- **Narrow API Scopes**: OAuth authorization requests ask only for storage and file operations (`drive.readonly`, `drive.file`, `Files.ReadWrite.All`) alongside PKCE (`S256`) and CSPRNG CSRF `state` validation, avoiding broad profile or identity permissions.
- **Never Logged**: Auth tokens, bearer headers, and refresh secrets are strictly excluded from console logs, test output, and SQLite transaction traces.

---

## 4. Multi-Account Isolation

Account configurations in `.utp_accounts.json` maintain discrete isolation:
- Separate token files are maintained for each connected account (`.utp_gdrive_work_token.json`, `.utp_gdrive_personal_token.json`).
- Concurrent transfers across different accounts operate on distinct HTTP client sessions and connection pools.

---

## 5. API Server Authentication Guard (Phase 3)

The UTP background daemon (`utpd`, bound by default to `127.0.0.1:8080`) enforces zero-trust access control across all HTTP endpoints:
- **Mandatory Authentication**: All `/api/*` endpoints require authentication using a Bearer token, API key, or (for `/api/stream` only) a short-lived resource-scoped HMAC-SHA256 `?ticket=`:
  - Header `Authorization: Bearer <token>`
  - Header `X-API-Key: <token>`
  - Signed stream ticket `POST /api/stream-tickets` → `?ticket=<exp>.<nonce>.<hmac>`
- **Rejection of Unauthenticated Calls**: Any call missing valid credentials is unconditionally rejected in constant time (`EVP_Digest` + `CRYPTO_memcmp`) with HTTP `401 Unauthorized` and `WWW-Authenticate: Bearer realm="utp"`.
- **Token Provisioning**: The server acquires its secret key from the `UTP_API_TOKEN` environment variable. If unset, it generates a cryptographically random 64-character hexadecimal token (`RAND_bytes(32)`) on startup and writes it with owner-only permissions (`0600` / user ACL) to `.utp_api_token` without logging it.
- **CORS Preflight Exemption**: HTTP `OPTIONS` requests are validated against loopback origins (or `UTP_CORS_ORIGIN`) without exposing credentials to untrusted origins.
