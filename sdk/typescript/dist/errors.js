/**
 * Universal Cloud Transfer Protocol (UTP) - Error Classes
 */
export class UTPError extends Error {
    statusCode;
    details;
    constructor(message, statusCode, details) {
        super(message);
        this.name = 'UTPError';
        this.statusCode = statusCode;
        this.details = details;
        Object.setPrototypeOf(this, new.target.prototype);
    }
}
export class UTPAuthError extends UTPError {
    constructor(message = 'Unauthorized: Invalid or missing API token') {
        super(message, 401);
        this.name = 'UTPAuthError';
    }
}
export class UTPNotFoundError extends UTPError {
    constructor(message = 'Transfer resource not found') {
        super(message, 404);
        this.name = 'UTPNotFoundError';
    }
}
export class UTPConflictError extends UTPError {
    constructor(message = 'Transfer already in a terminal state') {
        super(message, 409);
        this.name = 'UTPConflictError';
    }
}
export class UTPTransferFailedError extends UTPError {
    transferId;
    reason;
    constructor(transferId, reason) {
        super(`Transfer ${transferId} failed${reason ? `: ${reason}` : '.'}`);
        this.name = 'UTPTransferFailedError';
        this.transferId = transferId;
        this.reason = reason;
    }
}
export class UTPTimeoutError extends UTPError {
    constructor(message = 'Operation timed out waiting for transfer completion') {
        super(message);
        this.name = 'UTPTimeoutError';
    }
}
//# sourceMappingURL=errors.js.map