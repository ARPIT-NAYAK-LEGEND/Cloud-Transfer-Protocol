/**
 * Universal Cloud Transfer Protocol (UTP) - TypeScript Client
 */
import { UTPError, UTPAuthError, UTPNotFoundError, UTPConflictError, UTPTransferFailedError, UTPTimeoutError, } from './errors.js';
export class UTPClient {
    baseUrl;
    token;
    timeoutMs;
    fetchFn;
    /**
     * Initializes a new UTPClient instance.
     * @param options UTPClientOptions configuration object or base URL string
     */
    constructor(options = {}) {
        const opts = typeof options === 'string' ? { baseUrl: options } : options;
        const rawUrl = opts.baseUrl || 'http://localhost:8080';
        this.baseUrl = rawUrl.replace(/\/+$/, '');
        this.token = opts.token;
        this.timeoutMs = opts.timeoutMs ?? 30000;
        this.fetchFn = opts.fetchFn || globalThis.fetch.bind(globalThis);
    }
    /**
     * Initiates a transfer from source URI to destination URI in the background.
     * @param srcUri Source resource URI (e.g. gdrive://folder/file.bin or file:///path)
     * @param destUri Destination resource URI (e.g. onedrive://backup/file.bin)
     * @param options Optional transfer options
     * @returns Transfer ID of the initiated transfer
     */
    async transfer(srcUri, destUri, options) {
        const payload = {
            src: srcUri,
            dest: destUri,
        };
        if (options?.id)
            payload.id = options.id;
        if (options?.bandwidthLimit)
            payload.bandwidth = options.bandwidthLimit;
        const data = await this.request('POST', '/api/transfers', payload);
        return data.id;
    }
    /**
     * Retrieves the current state and byte progress of a transfer.
     * @param transferId The unique transfer ID
     */
    async getTransfer(transferId) {
        return await this.request('GET', `/api/transfers/${encodeURIComponent(transferId)}`);
    }
    /**
     * Lists recent transfers tracked by the UTP engine ledger.
     * @param limit Maximum number of records to return
     */
    async listTransfers(limit = 50) {
        return await this.request('GET', `/api/transfers?limit=${limit}`);
    }
    /**
     * Retrieves the §7 lifecycle event stream for a transfer.
     * @param transferId The unique transfer ID
     * @param limit Maximum number of events to return
     */
    async getEvents(transferId, limit = 200) {
        return await this.request('GET', `/api/transfers/${encodeURIComponent(transferId)}/events?limit=${limit}`);
    }
    /**
     * Requests a running transfer to pause between chunks, keeping all progress intact.
     * @param transferId The unique transfer ID
     */
    async pauseTransfer(transferId) {
        await this.request('POST', `/api/transfers/${encodeURIComponent(transferId)}/pause`);
    }
    /**
     * Requests a running transfer to cancel, discarding incomplete upload sessions.
     * @param transferId The unique transfer ID
     */
    async cancelTransfer(transferId) {
        await this.request('POST', `/api/transfers/${encodeURIComponent(transferId)}/cancel`);
    }
    /**
     * Polls the transfer until it reaches COMPLETED, FAILED, or CANCELLED state.
     * @param transferId The unique transfer ID
     * @param options Polling configuration, progress callbacks, and timeout
     */
    async waitForCompletion(transferId, options) {
        const pollInterval = options?.pollIntervalMs ?? 1000;
        const timeout = options?.timeoutMs;
        const startTime = Date.now();
        let lastBytes = -1;
        let seenEventCount = 0;
        while (true) {
            if (options?.signal?.aborted) {
                throw new UTPError('Waiting aborted by AbortSignal');
            }
            if (timeout && Date.now() - startTime > timeout) {
                throw new UTPTimeoutError(`Timed out waiting for transfer ${transferId} after ${timeout}ms`);
            }
            const transfer = await this.getTransfer(transferId);
            if (options?.onProgress && transfer.bytes_transferred !== lastBytes) {
                lastBytes = transfer.bytes_transferred;
                options.onProgress(transfer);
            }
            if (options?.onEvent) {
                try {
                    const events = await this.getEvents(transferId);
                    if (events.length > seenEventCount) {
                        for (let i = seenEventCount; i < events.length; i++) {
                            options.onEvent(events[i]);
                        }
                        seenEventCount = events.length;
                    }
                }
                catch {
                    // Event stream polling errors are non-fatal to waiting
                }
            }
            if (transfer.state === 'COMPLETED') {
                return transfer;
            }
            if (transfer.state === 'FAILED') {
                throw new UTPTransferFailedError(transferId);
            }
            if (transfer.state === 'CANCELLED') {
                throw new UTPError(`Transfer ${transferId} was cancelled`, 409);
            }
            await this.sleep(pollInterval, options?.signal);
        }
    }
    /**
     * Streams §7 lifecycle events asynchronously as they are emitted by the engine.
     * @param transferId The unique transfer ID
     * @param options Optional polling interval and cancellation signal
     */
    async *streamEvents(transferId, options) {
        const pollInterval = options?.pollIntervalMs ?? 500;
        let seenCount = 0;
        let terminal = false;
        while (!terminal) {
            if (options?.signal?.aborted)
                break;
            try {
                const [events, transfer] = await Promise.all([
                    this.getEvents(transferId),
                    this.getTransfer(transferId),
                ]);
                if (events.length > seenCount) {
                    for (let i = seenCount; i < events.length; i++) {
                        yield events[i];
                    }
                    seenCount = events.length;
                }
                if (['COMPLETED', 'FAILED', 'CANCELLED'].includes(transfer.state)) {
                    terminal = true;
                    break;
                }
            }
            catch (err) {
                if (options?.signal?.aborted)
                    break;
                throw err;
            }
            await this.sleep(pollInterval, options?.signal);
        }
    }
    async request(method, path, body) {
        const url = `${this.baseUrl}${path}`;
        const headers = {
            Accept: 'application/json',
        };
        if (body) {
            headers['Content-Type'] = 'application/json';
        }
        if (this.token) {
            headers['Authorization'] = `Bearer ${this.token}`;
        }
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);
        try {
            const response = await this.fetchFn(url, {
                method,
                headers,
                body: body ? JSON.stringify(body) : undefined,
                signal: controller.signal,
            });
            const responseText = await response.text();
            let responseJson;
            try {
                responseJson = responseText ? JSON.parse(responseText) : {};
            }
            catch {
                responseJson = { raw: responseText };
            }
            if (!response.ok) {
                const errorMsg = responseJson.error || responseText || `HTTP ${response.status}`;
                switch (response.status) {
                    case 401:
                        throw new UTPAuthError(errorMsg);
                    case 404:
                        throw new UTPNotFoundError(errorMsg);
                    case 409:
                        throw new UTPConflictError(errorMsg);
                    default:
                        throw new UTPError(errorMsg, response.status, responseJson);
                }
            }
            return responseJson;
        }
        catch (err) {
            if (err instanceof UTPError)
                throw err;
            if (err instanceof Error && err.name === 'AbortError') {
                throw new UTPTimeoutError(`Request to ${path} timed out after ${this.timeoutMs}ms`);
            }
            throw new UTPError(`Failed to communicate with UTP daemon at ${this.baseUrl}: ${err instanceof Error ? err.message : String(err)}`, undefined, err);
        }
        finally {
            clearTimeout(timeoutId);
        }
    }
    sleep(ms, signal) {
        return new Promise((resolve, reject) => {
            if (signal?.aborted) {
                return reject(new UTPError('Operation cancelled by AbortSignal'));
            }
            const timer = setTimeout(resolve, ms);
            signal?.addEventListener('abort', () => {
                clearTimeout(timer);
                reject(new UTPError('Operation cancelled by AbortSignal'));
            }, { once: true });
        });
    }
}
//# sourceMappingURL=client.js.map