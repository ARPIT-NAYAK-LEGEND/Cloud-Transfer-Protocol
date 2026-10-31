/**
 * Universal Cloud Transfer Protocol (UTP) - TypeScript Client
 */
import { Transfer, TransferEvent, TransferOptions, UTPClientOptions, WaitOptions } from './types.js';
export declare class UTPClient {
    private readonly baseUrl;
    private readonly token?;
    private readonly timeoutMs;
    private readonly fetchFn;
    /**
     * Initializes a new UTPClient instance.
     * @param options UTPClientOptions configuration object or base URL string
     */
    constructor(options?: UTPClientOptions | string);
    /**
     * Initiates a transfer from source URI to destination URI in the background.
     * @param srcUri Source resource URI (e.g. gdrive://folder/file.bin or file:///path)
     * @param destUri Destination resource URI (e.g. onedrive://backup/file.bin)
     * @param options Optional transfer options
     * @returns Transfer ID of the initiated transfer
     */
    transfer(srcUri: string, destUri: string, options?: TransferOptions): Promise<string>;
    /**
     * Retrieves the current state and byte progress of a transfer.
     * @param transferId The unique transfer ID
     */
    getTransfer(transferId: string): Promise<Transfer>;
    /**
     * Lists recent transfers tracked by the UTP engine ledger.
     * @param limit Maximum number of records to return
     */
    listTransfers(limit?: number): Promise<Transfer[]>;
    /**
     * Retrieves the §7 lifecycle event stream for a transfer.
     * @param transferId The unique transfer ID
     * @param limit Maximum number of events to return
     */
    getEvents(transferId: string, limit?: number): Promise<TransferEvent[]>;
    /**
     * Requests a running transfer to pause between chunks, keeping all progress intact.
     * @param transferId The unique transfer ID
     */
    pauseTransfer(transferId: string): Promise<void>;
    /**
     * Requests a running transfer to cancel, discarding incomplete upload sessions.
     * @param transferId The unique transfer ID
     */
    cancelTransfer(transferId: string): Promise<void>;
    /**
     * Polls the transfer until it reaches COMPLETED, FAILED, or CANCELLED state.
     * @param transferId The unique transfer ID
     * @param options Polling configuration, progress callbacks, and timeout
     */
    waitForCompletion(transferId: string, options?: WaitOptions): Promise<Transfer>;
    /**
     * Streams §7 lifecycle events asynchronously as they are emitted by the engine.
     * @param transferId The unique transfer ID
     * @param options Optional polling interval and cancellation signal
     */
    streamEvents(transferId: string, options?: {
        pollIntervalMs?: number;
        signal?: AbortSignal;
    }): AsyncIterable<TransferEvent>;
    private request;
    private sleep;
}
//# sourceMappingURL=client.d.ts.map