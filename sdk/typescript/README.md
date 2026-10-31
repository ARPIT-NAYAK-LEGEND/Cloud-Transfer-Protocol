# @utp/client

Official TypeScript / JavaScript SDK for the **Universal Cloud Transfer Protocol (UTP)**.

Stream files and folders across 10 heterogeneous cloud providers (Google Drive, Microsoft OneDrive, Amazon S3, Cloudflare R2, Backblaze B2, Azure Blob, Dropbox, Apple iCloud, WebDAV, and Local Disk) through high-speed volatile RAM ring buffers with zero disk footprint.

---

## Installation

```bash
npm install @utp/client
```

---

## Quickstart

```typescript
import { UTPClient } from '@utp/client';

// Connect to local or remote UTP daemon
const client = new UTPClient({
  baseUrl: 'http://localhost:8080',
  token: process.env.UTP_API_TOKEN, // Optional bearer auth token
});

async function main() {
  // 1. Initiate a transfer
  const transferId = await client.transfer(
    'gdrive://research/dataset.tar.gz',
    'onedrive://backup/dataset.tar.gz'
  );
  console.log(`Transfer started: ${transferId}`);

  // 2. Stream lifecycle events & monitor progress
  const finalTransfer = await client.waitForCompletion(transferId, {
    pollIntervalMs: 1000,
    onProgress: (t) => {
      const pct = t.size > 0 ? Math.round((t.bytes_transferred / t.size) * 100) : 0;
      console.log(`Progress: ${pct}% (${t.bytes_transferred} / ${t.size} bytes)`);
    },
    onEvent: (ev) => {
      console.log(`[Event ${ev.type}]: ${ev.payload}`);
    },
  });

  console.log('Transfer finished successfully:', finalTransfer);
}

main().catch(console.error);
```

---

## API Reference

### `new UTPClient(options?: UTPClientOptions | string)`

Options:
- `baseUrl` *(string)*: Daemon URL (defaults to `http://localhost:8080`).
- `token` *(string)*: Optional Bearer authentication token.
- `timeoutMs` *(number)*: Request timeout in milliseconds (defaults to `30000`).
- `fetchFn` *(function)*: Optional custom `fetch` implementation.

### `client.transfer(srcUri: string, destUri: string, options?: TransferOptions): Promise<string>`
Initiates a background transfer and returns the unique transfer ID.

### `client.getTransfer(transferId: string): Promise<Transfer>`
Retrieves the real-time status and byte progression.

### `client.listTransfers(limit?: number): Promise<Transfer[]>`
Retrieves recent transfers tracked in the persistent SQLite ledger.

### `client.getEvents(transferId: string, limit?: number): Promise<TransferEvent[]>`
Fetches §7 lifecycle event logs for a given transfer.

### `client.pauseTransfer(transferId: string): Promise<void>`
Requests a running transfer to pause safely between chunks without losing progress.

### `client.cancelTransfer(transferId: string): Promise<void>`
Aborts a transfer and rolls back incomplete sessions.

### `client.waitForCompletion(transferId: string, options?: WaitOptions): Promise<Transfer>`
Polls the daemon until terminal state (`COMPLETED` or `FAILED`).

### `client.streamEvents(transferId: string): AsyncIterable<TransferEvent>`
Consumes engine events as an `AsyncIterable` stream.

---

## License

MIT © UTP Core Contributors
