# UTP Security & Credential Hygiene Specification

The Universal Cloud Transfer Protocol is designed with zero-trust and client-side privacy as core tenets.

## 1. Zero-Disk Memory Footprint (Privacy Guarantee)

UTP streams bytes directly from cloud-to-cloud through volatile RAM ring buffers.
- **No Temporary Staging**: Transferred data is never cached, buffered, or staged to local hard drives or SSDs.
- **Transient Memory Scrubber**: Buffer memory is overwritten and freed immediately after each chunk commit.
- **Sensitive Payload Immunity**: Moving sensitive financial records, private photos, or trade secrets leaves no recoverable trace on the host file system.

## 2. In-Transit Encryption

- All provider adapters communicate strictly via HTTPS with TLS 1.3 or TLS 1.2.
- Mutual verification of remote server certificates using platform root certificate bundles.
- Perfect Forward Secrecy (PFS) cipher suites prioritized for all data plane sockets.

## 3. Credential Hygiene & Token Storage

Cloud tokens are sensitive credentials:
- **Local Isolation**: Tokens are stored exclusively in user-scoped files (`.utp_*_token.json`) located in the application directory.
- **Narrow API Scopes**: OAuth authorization requests ask only for storage and file operations (`drive.file`, `Files.ReadWrite.All`), avoiding broad profile or identity permissions.
- **Obfuscation & Encryption**: Hardcoded client IDs and secrets utilize XOR-masked byte arrays to prevent casual string extraction in binaries.
- **Never Logged**: Auth tokens, bearer headers, and refresh secrets are strictly excluded from console logs, test output, and SQLite transaction traces.

## 4. Multi-Account Isolation

Account configurations in `.utp_accounts.json` maintain discrete isolation:
- Separate token files are maintained for each connected account (`.utp_gdrive_work_token.json`, `.utp_gdrive_personal_token.json`).
- Concurrent transfers across different accounts operate on distinct HTTP client sessions and connection pools.
