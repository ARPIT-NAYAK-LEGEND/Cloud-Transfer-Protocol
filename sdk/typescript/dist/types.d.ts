/**
 * Universal Cloud Transfer Protocol (UTP) - Type Definitions
 * Matches the C++ core engine state machine (§5) and lifecycle event stream (§7).
 */
export type TransferState = 'CREATED' | 'AUTHENTICATING' | 'DISCOVERING' | 'NEGOTIATING' | 'PLANNING' | 'TRANSFERRING' | 'VERIFYING' | 'COMMITTING' | 'COMPLETED' | 'PAUSED' | 'RETRYING' | 'REPAIRING' | 'CANCELLED' | 'FAILED' | 'UNKNOWN';
export type EventType = 'TRANSFER_CREATED' | 'TRANSFER_STARTED' | 'OBJECT_STARTED' | 'CHUNK_COMPLETED' | 'OBJECT_COMPLETED' | 'TRANSFER_PAUSED' | 'TRANSFER_RESUMED' | 'TRANSFER_VERIFIED' | 'TRANSFER_COMPLETED' | 'TRANSFER_FAILED' | 'SYNC_STARTED' | 'SYNC_PLAN_READY' | 'SYNC_ITEM_COMPLETED' | 'SYNC_CONFLICT_RESOLVED' | 'SYNC_COMPLETED' | 'SYNC_FAILED' | 'MULTI_TRANSFER_STARTED' | 'MULTI_DEST_CHUNK_COMPLETED' | 'MULTI_DEST_COMPLETED' | 'MULTI_DEST_FAILED' | 'MULTI_TRANSFER_COMPLETED' | 'MULTI_TRANSFER_FAILED';
export interface Transfer {
    id: string;
    source: string;
    destination: string;
    state: TransferState;
    size: number;
    bytes_transferred: number;
}
export interface TransferEvent {
    transfer_id: string;
    type: EventType | string;
    payload: string;
    timestamp: number;
}
export interface TransferOptions {
    id?: string;
    bandwidthLimit?: number;
}
export interface UTPClientOptions {
    /**
     * Base URL of the UTP daemon API server (defaults to http://localhost:8080)
     */
    baseUrl?: string;
    /**
     * Bearer authentication token. Required if utpd requires auth.
     */
    token?: string;
    /**
     * Request timeout in milliseconds (defaults to 30000 ms)
     */
    timeoutMs?: number;
    /**
     * Custom fetch implementation (optional, defaults to global fetch)
     */
    fetchFn?: typeof fetch;
}
export interface WaitOptions {
    /**
     * Interval between polling status requests in milliseconds (defaults to 1000 ms)
     */
    pollIntervalMs?: number;
    /**
     * Maximum duration to wait before timing out in milliseconds (defaults to unlimited)
     */
    timeoutMs?: number;
    /**
     * Progress callback invoked whenever bytes_transferred updates
     */
    onProgress?: (transfer: Transfer) => void;
    /**
     * Event callback invoked whenever a new §7 lifecycle event occurs
     */
    onEvent?: (event: TransferEvent) => void;
    /**
     * Optional AbortSignal to cancel waiting
     */
    signal?: AbortSignal;
}
export type SyncPolicy = 'FAIL' | 'OVERWRITE' | 'RENAME' | 'SKIP' | 'NEWER_WINS';
export type MultiTransferPolicy = 'ALL_OR_NOTHING' | 'BEST_EFFORT';
//# sourceMappingURL=types.d.ts.map