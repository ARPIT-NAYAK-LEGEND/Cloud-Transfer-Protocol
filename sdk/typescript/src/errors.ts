/**
 * Universal Cloud Transfer Protocol (UTP) - Error Classes
 */

export class UTPError extends Error {
  public statusCode?: number;
  public details?: unknown;

  constructor(message: string, statusCode?: number, details?: unknown) {
    super(message);
    this.name = 'UTPError';
    this.statusCode = statusCode;
    this.details = details;
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

export class UTPAuthError extends UTPError {
  constructor(message: string = 'Unauthorized: Invalid or missing API token') {
    super(message, 401);
    this.name = 'UTPAuthError';
  }
}

export class UTPNotFoundError extends UTPError {
  constructor(message: string = 'Transfer resource not found') {
    super(message, 404);
    this.name = 'UTPNotFoundError';
  }
}

export class UTPConflictError extends UTPError {
  constructor(message: string = 'Transfer already in a terminal state') {
    super(message, 409);
    this.name = 'UTPConflictError';
  }
}

export class UTPTransferFailedError extends UTPError {
  public transferId: string;
  public reason?: string;

  constructor(transferId: string, reason?: string) {
    super(`Transfer ${transferId} failed${reason ? `: ${reason}` : '.'}`);
    this.name = 'UTPTransferFailedError';
    this.transferId = transferId;
    this.reason = reason;
  }
}

export class UTPTimeoutError extends UTPError {
  constructor(message: string = 'Operation timed out waiting for transfer completion') {
    super(message);
    this.name = 'UTPTimeoutError';
  }
}
