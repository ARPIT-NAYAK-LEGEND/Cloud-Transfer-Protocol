import time
import requests
from enum import Enum
from typing import Optional, Dict, Any

class TransferState(Enum):
    CREATED = "CREATED"
    PLANNING = "PLANNING"
    TRANSFERRING = "TRANSFERRING"
    VERIFYING = "VERIFYING"
    COMMITTING = "COMMITTING"
    COMPLETED = "COMPLETED"
    FAILED = "FAILED"

class TransferError(Exception):
    pass

class UTPClient:
    """
    Client for interacting with the Universal Cloud Transfer Protocol (UTP) Daemon.

    NOTE: This client library communicates with the UTP REST API daemon (`utpd`),
    which is an EXPERIMENTAL post-MVP surface pending multi-cloud DoD gates.
    """
    def __init__(self, host: str = "http://localhost:8080"):
        self.base_url = f"{host.rstrip('/')}/api"

    def transfer(self, src_uri: str, dest_uri: str) -> str:
        """
        Initiates a transfer from the source URI to the destination URI.
        Returns the transfer ID if available, or a success message.
        """
        resp = requests.post(f"{self.base_url}/transfers", json={
            "src": src_uri,
            "dest": dest_uri
        })
        
        if resp.status_code not in (200, 202):
            raise TransferError(f"Failed to initiate transfer: {resp.text}")
            
        data = resp.json()
        return data.get("id", "Transfer Initiated in Background")

    def get_status(self, transfer_id: str) -> Dict[str, Any]:
        """
        Retrieves the real-time status of a transfer.
        """
        resp = requests.get(f"{self.base_url}/transfers/{transfer_id}")
        if resp.status_code != 200:
            raise TransferError(f"Failed to get status for {transfer_id}: {resp.text}")
        return resp.json()

    def wait_until_complete(self, transfer_id: str, poll_interval: float = 2.0) -> Dict[str, Any]:
        """
        Blocks and polls the daemon until the transfer reaches COMPLETED or FAILED state.
        """
        while True:
            status = self.get_status(transfer_id)
            state_str = status.get("state", "UNKNOWN")
            
            if state_str == TransferState.COMPLETED.value:
                return status
            if state_str == TransferState.FAILED.value:
                raise TransferError(f"Transfer {transfer_id} failed.")
                
            time.sleep(poll_interval)
