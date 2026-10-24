# UTP Capabilities Specification

Capability negotiation is a fundamental tenet of the Universal Cloud Transfer Protocol (UTP). The UTP Engine never assumes what a cloud provider or filesystem supports; it queries both the source and destination adapters before generating a `TransferPlan`.

## ProviderCapabilities Contract

Each storage provider exposes its capabilities through the standard struct:

```cpp
struct ProviderCapabilities {
    bool ranged_read;               // Supports HTTP byte-range requests (RFC 7233)
    bool resumable_upload;          // Supports stateful session upload protocols
    bool multipart_upload;          // Supports out-of-order parallel chunk uploads
    bool server_side_copy;          // Supports direct cloud-to-cloud server-side copy
    bool checksum;                  // Computes and validates cryptographic checksums
    bool metadata;                  // Preserves timestamps, MIME types, and extended attributes
    bool directory_support;         // Native hierarchical folders and traversal
    uint64_t max_chunk_size;        // Maximum chunk byte limit allowed by provider API
    uint32_t max_parallel_requests; // Maximum concurrent worker threads allowed
};
```

## Negotiation Algorithm

During the `PLANNING` phase of a transfer, the engine executes dynamic capability resolution:

1. **Zero-Bandwidth Fast Path (Server-Side Copy)**:
   If both source and destination share the same underlying provider scheme (or multi-account cross-sharing is permitted) and `server_side_copy == true`:
   - Engine attempts cloud-native server copy (`copy_shared_file`).
   - If successful, bytes never traverse the local client machine, completing in seconds.

2. **Adaptive Chunk Sizing**:
   When streaming over the network, chunk sizes scale dynamically based on payload magnitude:
   - `< 20 MB`: 2.5 MB chunks (minimizes latency overhead).
   - `20 MB - 100 MB`: 10 MB chunks.
   - `100 MB - 1 GB`: 20 MB chunks.
   - `> 1 GB`: 40 MB chunks (maximizes TCP window throughput).

3. **Cap Clamping**:
   The calculated chunk size is strictly bounded by:
   ```cpp
   effective_chunk_size = std::min(calculated_chunk_size, 
                                   std::min(source.max_chunk_size, dest.max_chunk_size));
   ```

4. **Parallelism Allocation**:
   Worker pool concurrency is bounded by:
   ```cpp
   worker_count = std::min(source.max_parallel_requests, dest.max_parallel_requests);
   ```

## Matrix of Provider Capabilities

| Provider | Ranged Read | Resumable Upload | Multipart | Server Copy | Checksum | Max Chunk | Parallel |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Local File** | Yes | Yes | Yes | Yes | SHA-256 | 64 MB | 16 |
| **Google Drive** | Yes | Yes | No | Yes | MD5 / SHA-256 | 40 MB | 8 |
| **OneDrive** | Yes | Yes | No | Yes | QuickXor / SHA-256 | 60 MB | 4 |
| **Dropbox** | Yes | Yes | Yes | Yes | Dropbox Content Hash | 150 MB | 8 |
| **Apple iCloud** | Yes | Yes | No | Yes | SHA-256 | 32 MB | 4 |
| **Amazon S3** | Yes | Yes | Yes | Yes | MD5 / SHA-256 | 5 GB | 16 |
| **Cloudflare R2** | Yes | Yes | Yes | Yes | MD5 / SHA-256 | 5 GB | 16 |
| **Backblaze B2** | Yes | Yes | Yes | Yes | SHA-1 / SHA-256 | 5 GB | 16 |
| **Azure Blob** | Yes | Yes | Yes | Yes | MD5 / SHA-256 | 100 MB | 8 |
| **WebDAV / Nextcloud** | Yes | Yes | No | Yes | SHA-256 | 64 MB | 4 |
