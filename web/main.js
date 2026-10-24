const API_BASE = 'http://localhost:8080/api';
let activePollers = new Map();

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function renderTransferCard(container, transfer) {
  let card = document.getElementById(`transfer-${transfer.id}`);
  const pct = transfer.size > 0 ? Math.min(100, Math.floor((transfer.bytes_transferred / transfer.size) * 100)) : 0;
  const state = transfer.state || 'PENDING';

  if (!card) {
    card = document.createElement('div');
    card.id = `transfer-${transfer.id}`;
    card.className = 'transfer-item';
    container.prepend(card);
  }

  card.innerHTML = `
    <div class="transfer-header">
      <span class="transfer-id">${transfer.id}</span>
      <span class="transfer-status status-${state}">${state}</span>
    </div>
    <div style="font-size: 0.75rem; color: #94a3b8; margin: 4px 0 8px 0; word-break: break-all;">
      ${transfer.source || ''} ➔ ${transfer.destination || ''}
    </div>
    <div class="transfer-meta">
      <span class="transfer-progress-text">${formatBytes(transfer.bytes_transferred)} / ${formatBytes(transfer.size)}</span>
      <span class="transfer-percentage">${pct}%</span>
    </div>
    <div class="progress-track">
      <div class="progress-fill" style="width: ${pct}%"></div>
    </div>
  `;
}

async function loadExistingTransfers() {
  const container = document.getElementById('transfersContainer');
  try {
    const res = await fetch(`${API_BASE}/transfers`);
    if (!res.ok) return;
    const data = await res.json();
    if (Array.isArray(data) && data.length > 0) {
      container.innerHTML = '';
      data.forEach(t => renderTransferCard(container, t));
    }
  } catch (err) {
    console.debug('Daemon not reachable on initial load:', err);
  }
}

function startPollingTransfer(transferId) {
  if (activePollers.has(transferId)) return;
  const container = document.getElementById('transfersContainer');

  const interval = setInterval(async () => {
    try {
      const res = await fetch(`${API_BASE}/transfers/${transferId}`);
      if (!res.ok) return;
      const data = await res.json();

      const empty = container.querySelector('.empty-state');
      if (empty) empty.remove();

      renderTransferCard(container, data);

      if (data.state === 'COMPLETED' || data.state === 'FAILED' || data.state === 'CANCELLED') {
        clearInterval(interval);
        activePollers.delete(transferId);
      }
    } catch (e) {
      console.error(`Error polling ${transferId}:`, e);
    }
  }, 500);

  activePollers.set(transferId, interval);
}

document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('transferForm');
  const container = document.getElementById('transfersContainer');

  loadExistingTransfers();

  form.addEventListener('submit', async (e) => {
    e.preventDefault();

    const src = document.getElementById('srcUri').value.trim();
    const dest = document.getElementById('destUri').value.trim();
    const btn = form.querySelector('button');
    const originalText = btn.innerText;

    btn.innerText = 'Initiating...';
    btn.disabled = true;

    try {
      const response = await fetch(`${API_BASE}/transfers`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ src, dest })
      });

      if (!response.ok) {
        const errJson = await response.json().catch(() => ({}));
        throw new Error(errJson.error || `HTTP ${response.status}`);
      }

      const data = await response.json();
      const transferId = data.id;

      const empty = container.querySelector('.empty-state');
      if (empty) empty.remove();

      renderTransferCard(container, {
        id: transferId,
        source: src,
        destination: dest,
        state: 'CREATED',
        size: 0,
        bytes_transferred: 0
      });

      startPollingTransfer(transferId);
      form.reset();
    } catch (error) {
      console.error('Transfer failed:', error);
      alert(`Transfer initiation failed: ${error.message}`);
    } finally {
      btn.innerText = originalText;
      btn.disabled = false;
    }
  });
});
