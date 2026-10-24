# UTP Integrity Verification Specification

A successful network request is not proof of a successful transfer. Data corruption can happen across TLS proxies, cloud storage gateways, or edge servers. UTP enforces cryptographic verification across all transfers.

## Core Principle: End-to-End Cryptographic Proof

Every payload transferred via UTP undergoes real-time, incremental SHA-256 verification.

```
Source Stream ──[EVP_DigestUpdate]──▶ Source SHA-256 ─┐
                                                       ├─▶ Compare at VERIFYING
Dest Stream   ──[EVP_DigestUpdate]──▶ Dest SHA-256   ─┘
```

## Zero-Memory Streaming Verification

To uphold UTP's strict memory discipline (§10 of `CLAUDE.md`), files are **never** loaded into memory to compute a hash. Instead:

1. When a chunk buffer is read from the source provider, `EVP_DigestUpdate` is invoked directly on the in-flight chunk buffer.
2. When the destination provider processes the chunk write, its corresponding cryptographic context is updated.
3. Upon reaching the end-of-file, both digests are finalized via `EVP_DigestFinal_ex`.
4. The transfer transitions to `VERIFYING`:
   - If both hex digests match identically, the transfer transitions to `COMMITTING` and then `COMPLETED`.
   - If the hashes mismatch, the transfer is immediately rejected, marked `FAILED`, and the destination write session is aborted.

## Chunk-Level Checksums in Persistence

Every chunk within the SQLite ledger (`utp.db`) can hold an optional `checksum` column:

```sql
chunks (
    transfer_id TEXT,
    chunk_index INTEGER,
    offset INTEGER,
    size INTEGER,
    state TEXT,
    checksum TEXT,
    attempts INTEGER
);
```

When individual chunks are retried or resumed, matching chunk checksums guarantee that resume skips only bit-identical, verified slices.
