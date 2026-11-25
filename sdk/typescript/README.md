# `@utp/client`

Official TypeScript / JavaScript SDK for the **Universal Cloud Transfer Protocol (UTP)** daemon (`utpd`).

Stream files across cloud providers (`gdrive://`, `onedrive://`, `s3://`, `r2://`, `b2://`, `azure://`, `dropbox://`, `icloud://`, `webdav://`, `file://`) and ingest direct `http://`/`https://` web URLs through volatile RAM ring buffers with zero local disk footprint.

---

## Installation

```bash
npm install @utp/client
```

Requires Node.js `>= 18.0.0` (uses native `fetch` unless a custom `fetchFn` is provided).

---

## Quickstart

```typescript
import { UTPClient } from '@utp/client';

// Connect to local or remote UTP daemon (`utp daemon start` or `utpd`)
const client = new UTPClient({
  baseUrl: 'http://localhost:8080',
  token: process.env.UTP_API_TOKEN, // Optional Bearer auth token
});

async function main() {
  // 1. Initiate a zero-disk cloud-to-cloud or HTTPS-to-cloud transfer
  const transferId = await client.transfer(
    'gdrive://research/dataset.tar.gz',
    'onedrive://backup/dataset.tar.gz'
  );
  console.log(`Transfer started: ${transferId}`);

  // 2. Monitor progress & lifecycle events
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

  console.log('Transfer finished:', finalTransfer);

  // 3. Build an on-demand HTTP 206 streaming or direct download URL
  const streamUrl = client.getStreamUrl('gdrive://movies/demo.mp4');
  const downloadUrl = client.getStreamUrl('gdrive://movies/demo.mp4', {
    download: true,
    filename: 'demo.mp4',
  });
  console.log({ streamUrl, downloadUrl });
}

main().catch(console.error);
```

---

## API Reference

### `new UTPClient(options?: UTPClientOptions | string)`

- `baseUrl` *(string)*: Daemon URL (defaults to `http://localhost:8080`).
- `token` *(string)*: Bearer authentication token (matches `.utp_api_token` / `UTP_API_TOKEN`).
- `timeoutMs` *(number)*: Request timeout in milliseconds (defaults to `30000`).
- `fetchFn` *(function)*: Optional custom `fetch` implementation.

### `client.transfer(srcUri: string, destUri: string, options?: TransferOptions): Promise<string>`
Initiates a background transfer (`gdrive://`, `onedrive://`, `https://`, `file://`, etc.) and returns the unique transfer ID.

### `client.getTransfer(transferId: string): Promise<Transfer>`
Retrieves real-time transfer state and byte progression.

### `client.listTransfers(limit?: number): Promise<Transfer[]>`
Lists recent transfers recorded in the persistent SQLite ledger (`utp.db`).

### `client.getEvents(transferId: string, limit?: number): Promise<TransferEvent[]>`
Fetches lifecycle event logs for a given transfer.

### `client.pauseTransfer(transferId: string): Promise<void>`
Requests a running transfer to pause safely at the next chunk boundary without losing progress.

### `client.cancelTransfer(transferId: string): Promise<void>`
Aborts a transfer and discards partial upload state.

### `client.waitForCompletion(transferId: string, options?: WaitOptions): Promise<Transfer>`
Polls the daemon until terminal state (`COMPLETED`, `FAILED`, or `CANCELLED`).

### `client.streamEvents(transferId: string, options?: { pollIntervalMs?: number; signal?: AbortSignal }): AsyncIterable<TransferEvent>`
Consumes engine lifecycle events as an async iterable stream.

### `client.getSignedStreamUrl(uriOrId: string, options?: { download?: boolean; filename?: string }): Promise<string>`
Issues a short-lived HMAC-SHA256 signed stream ticket via `POST /api/stream-tickets` (`Authorization: Bearer`) and builds a `/api/stream?ticket=...` URL without exposing the master bearer token in browser history or logs.

### `client.getStreamUrl(uriOrId: string, options?: { download?: boolean; filename?: string }): string`
Synchronously constructs a `/api/stream` URL (requires `--allow-query-token` on `utpd` if a `token` query parameter is used instead of `getSignedStreamUrl`).

---

## License

[MIT](https://github.com/ARPIT-NAYAK-LEGEND/Cloud-Transfer-Protocol/blob/PROJECTS/LICENSE) © Arpit Nayak
