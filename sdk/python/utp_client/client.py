import time
import urllib.parse
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

    def __init__(
        self,
        host: str = "http://127.0.0.1:8080",
        token: Optional[str] = None,
        timeout: float = 30.0,
    ):
        self.base_url = f"{host.rstrip('/')}/api"
        self.token = token
        self.timeout = timeout

    def _headers(self) -> Dict[str, str]:
        headers: Dict[str, str] = {"Accept": "application/json"}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        return headers

    def transfer(self, src_uri: str, dest_uri: str) -> str:
        """
        Initiates a transfer from the source URI to the destination URI.
        Returns the transfer ID.
        """
        resp = requests.post(
            f"{self.base_url}/transfers",
            json={"src": src_uri, "dest": dest_uri},
            headers=self._headers(),
            timeout=self.timeout,
        )

        if resp.status_code not in (200, 202):
            raise TransferError(f"Failed to initiate transfer ({resp.status_code}): {resp.text}")

        data = resp.json()
        return data.get("id", "Transfer Initiated in Background")

    def get_status(self, transfer_id: str) -> Dict[str, Any]:
        """
        Retrieves the real-time status of a transfer.
        """
        safe_id = urllib.parse.quote(transfer_id, safe="")
        resp = requests.get(
            f"{self.base_url}/transfers/{safe_id}",
            headers=self._headers(),
            timeout=self.timeout,
        )
        if resp.status_code != 200:
            raise TransferError(f"Failed to get status for {transfer_id} ({resp.status_code}): {resp.text}")
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

    def get_stream_url(
        self,
        uri_or_id: str,
        download: bool = False,
        filename: Optional[str] = None,
    ) -> str:
        """
        Constructs the on-demand zero-disk streaming URL for a cloud resource or transfer ID.
        Can be used directly in VLC, MPV, or media players.
        """
        params: Dict[str, str] = {}
        if "://" in uri_or_id:
            params["uri"] = uri_or_id
        else:
            params["id"] = uri_or_id
        if self.token:
            params["token"] = self.token
        if download:
            params["download"] = "true"
        if filename:
            params["name"] = filename
        return f"{self.base_url}/stream?{urllib.parse.urlencode(params)}"

    def create_signed_stream_url(
        self,
        uri_or_id: str,
        download: bool = False,
        filename: Optional[str] = None,
    ) -> str:
        """
        Requests a short-lived signed stream ticket via Authorization header auth
        so the master bearer token never appears in the streaming URL.
        """
        body: Dict[str, str] = {"uri": uri_or_id} if "://" in uri_or_id else {"id": uri_or_id}
        resp = requests.post(
            f"{self.base_url}/stream-tickets",
            json=body,
            headers=self._headers(),
            timeout=self.timeout,
        )
        if resp.status_code != 200:
            raise TransferError(f"Failed to issue stream ticket ({resp.status_code}): {resp.text}")
        ticket = resp.json().get("ticket", "")
        if not ticket:
            raise TransferError("Daemon returned empty stream ticket")

        params: Dict[str, str] = {}
        if "://" in uri_or_id:
            params["uri"] = uri_or_id
        else:
            params["id"] = uri_or_id
        params["ticket"] = ticket
        if download:
            params["download"] = "true"
        if filename:
            params["name"] = filename
        return f"{self.base_url}/stream?{urllib.parse.urlencode(params)}"
