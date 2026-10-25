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

1. When a chunk buffer is read from the source provider, `EVP_DigestUpdate` is invoked directly on the in-flight chunk buffer, accumulating the source SHA-256 incrementally.
2. On resume, already-`COMPLETE` chunks are re-read from the source *solely* to keep the source digest complete — they are hashed but never re-written.
3. After the destination `commit()`, the engine streams the committed object back through a bounded buffer (another `EVP_DigestUpdate` loop) to compute the destination SHA-256. This verifies what the destination actually stored, not merely what we sent.
4. The transfer transitions to `VERIFYING`: both digests are finalized via `EVP_DigestFinal_ex` and compared.
   - If they match, the transfer transitions to `COMPLETED`.
   - If they differ, the transfer is marked `FAILED` and the error surfaces to the caller.
   - If the destination cannot be read back, verification is skipped with a warning rather than claiming success.

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
