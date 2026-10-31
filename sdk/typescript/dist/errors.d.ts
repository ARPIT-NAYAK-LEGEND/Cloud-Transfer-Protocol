/**
 * Universal Cloud Transfer Protocol (UTP) - Error Classes
 */
export declare class UTPError extends Error {
    statusCode?: number;
    details?: unknown;
    constructor(message: string, statusCode?: number, details?: unknown);
}
export declare class UTPAuthError extends UTPError {
    constructor(message?: string);
}
export declare class UTPNotFoundError extends UTPError {
    constructor(message?: string);
}
export declare class UTPConflictError extends UTPError {
    constructor(message?: string);
}
export declare class UTPTransferFailedError extends UTPError {
    transferId: string;
    reason?: string;
    constructor(transferId: string, reason?: string);
}
export declare class UTPTimeoutError extends UTPError {
    constructor(message?: string);
}
//# sourceMappingURL=errors.d.ts.map