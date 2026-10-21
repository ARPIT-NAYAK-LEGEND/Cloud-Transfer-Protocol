# UTP (Universal Cloud Transfer Protocol) - Experimental Specification

## 1. Introduction
The Universal Cloud Transfer Protocol (UTP) is a provider-independent protocol designed to standardize the transfer, resumption, and integrity verification of files across heterogeneous storage systems (e.g., Local Disk, Google Drive, OneDrive, Amazon S3, Azure Blob Storage).

UTP acts as an abstraction layer above provider-specific REST APIs. It establishes a uniform standard for:
1. Negotiating capabilities (Chunking, ranged reads, multipart uploads).
2. Identifying cloud resources universally via UTP URIs.
3. Managing state for resumable transfers across disconnected sessions.
4. Verifying data integrity through cryptographic checksums.

## 2. Core Components

### 2.1 Storage Providers
A Storage Provider is an adapter that conforms to the UTP interface, providing abstract operations like `stat`, `openRead`, `openWrite`, `read`, `write`, and `commit`. Providers expose their constraints via a Capabilities matrix.

### 2.2 Capability Matrix
Providers must declare their capabilities to the UTP Engine during the planning phase:
- `ranged_read`: Ability to fetch arbitrary byte ranges.
- `resumable_upload`: Support for stateful upload sessions.
- `multipart_upload`: Support for out-of-order part uploads.
- `checksum`: Integrity verification mechanisms.
- `max_chunk_size`: The maximum byte limit for a single chunk.

### 2.3 State Management
UTP mandates a durable state machine (e.g., stored in SQLite) to persist the transfer configuration, plan, and progress. This allows the protocol to seamlessly survive process crashes or network failures and resume exactly where it left off.

## 3. Protocol Flow
1. **Initiation**: The source and destination URIs are parsed.
2. **Planning**: The engine queries both providers' capabilities and constructs a `ChunkPlan`.
3. **Execution**: Chunks are read from the source and dispatched concurrently to the destination.
4. **Verification**: An overarching cryptographic hash (e.g., SHA-256) ensures end-to-end integrity.
5. **Commitment**: The destination provider commits the transfer, finalizing the remote file.

<!-- Security spec verified -->
