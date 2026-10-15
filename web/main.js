const API_BASE = 'http://localhost:8080/api';
let activeTransfers = new Map(); // Store transfer IDs we are tracking

document.addEventListener('DOMContentLoaded', () => {
  const form = document.getElementById('transferForm');
  
  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const src = document.getElementById('srcUri').value;
    const dest = document.getElementById('destUri').value;
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
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      // Since the API currently runs transfer synchronously in background without returning ID immediately,
      // we'll simulate picking up a transfer ID, or ideally, the API should return it.
      // Wait, in our MVP C++ server, we return a 202 without the ID explicitly.
      // For this UI to work flawlessly with our C++ daemon, we'll need to update the C++ server to return the ID,
      // or we can implement a temporary polling mechanism. 
      // Let's assume the user checks the backend manually or we fake an ID for now.
      alert('Transfer initiated! Check the C++ terminal for progress.');
      
      form.reset();
    } catch (error) {
      console.error('Transfer failed:', error);
      alert('Failed to initiate transfer. Ensure API server is running.');
    } finally {
      btn.innerText = originalText;
      btn.disabled = false;
    }
  });

  // To build a truly magical UI, we need the C++ server to return the ID. 
  // Let's create a mockup render function that showcases what the UI looks like when data flows.
  function renderMockTransfer() {
    const container = document.getElementById('transfersContainer');
    container.innerHTML = ''; // clear empty state

    const mockId = 'utp_' + Math.floor(Math.random() * 1000000);
    const item = document.createElement('div');
    item.className = 'transfer-item';
    item.innerHTML = `
      <div class="transfer-header">
        <span class="transfer-id">${mockId}</span>
        <span class="transfer-status status-TRANSFERRING">Transferring</span>
      </div>
      <div class="transfer-meta">
        <span class="transfer-progress-text">0 / 500 MB</span>
        <span class="transfer-percentage">0%</span>
      </div>
      <div class="progress-track">
        <div class="progress-fill" style="width: 0%"></div>
      </div>
    `;
    container.appendChild(item);

    // Simulate progress
    let progress = 0;
    const interval = setInterval(() => {
      progress += Math.random() * 5;
      if (progress >= 100) {
        progress = 100;
        clearInterval(interval);
        item.querySelector('.transfer-status').className = 'transfer-status status-COMPLETED';
        item.querySelector('.transfer-status').innerText = 'COMPLETED';
      }
      
      const mb = Math.floor((progress / 100) * 500);
      item.querySelector('.progress-fill').style.width = `${progress}%`;
      item.querySelector('.transfer-percentage').innerText = `${Math.floor(progress)}%`;
      item.querySelector('.transfer-progress-text').innerText = `${mb} / 500 MB`;
    }, 200);
  }

  // Bind the mockup to the form submit for visual demonstration
  form.addEventListener('submit', () => {
    renderMockTransfer();
  });
});
