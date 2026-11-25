import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import {
  UTPClient,
  UTPAuthError,
  UTPNotFoundError,
  UTPConflictError,
  UTPTransferFailedError,
} from '../dist/index.js';

describe('UTPClient SDK Unit & Mock Battery', () => {
  test('initializes with default options and token', () => {
    const client = new UTPClient({ baseUrl: 'http://localhost:8080', token: 'secret-tok' });
    assert.ok(client);
  });

  test('transfer() issues POST /api/transfers and returns id', async () => {
    let requestedUrl = '';
    let requestMethod = '';
    let requestHeaders = {};
    let requestBody = '';

    const mockFetch = async (url, init) => {
      requestedUrl = url.toString();
      requestMethod = init.method;
      requestHeaders = init.headers;
      requestBody = init.body;

      return {
        ok: true,
        status: 202,
        text: async () => JSON.stringify({ status: 'accepted', id: 'utp_test_12345' }),
      };
    };

    const client = new UTPClient({
      baseUrl: 'http://localhost:8080',
      token: 'test-token',
      fetchFn: mockFetch,
    });

    const id = await client.transfer('gdrive://doc.pdf', 'onedrive://doc.pdf', {
      bandwidthLimit: 5000000,
    });

    assert.equal(id, 'utp_test_12345');
    assert.equal(requestedUrl, 'http://localhost:8080/api/transfers');
    assert.equal(requestMethod, 'POST');
    assert.equal(requestHeaders['Authorization'], 'Bearer test-token');
    assert.match(requestBody, /"src":"gdrive:\/\/doc\.pdf"/);
    assert.match(requestBody, /"dest":"onedrive:\/\/doc\.pdf"/);
    assert.match(requestBody, /"bandwidth":5000000/);
  });

  test('getTransfer() retrieves transfer state', async () => {
    const mockFetch = async (url) => {
      assert.equal(url.toString(), 'http://localhost:8080/api/transfers/utp_abc');
      return {
        ok: true,
        status: 200,
        text: async () =>
          JSON.stringify({
            id: 'utp_abc',
            source: 'gdrive://file.bin',
            destination: 's3://bucket/file.bin',
            state: 'TRANSFERRING',
            size: 104857600,
            bytes_transferred: 52428800,
          }),
      };
    };

    const client = new UTPClient({ baseUrl: 'http://localhost:8080', fetchFn: mockFetch });
    const res = await client.getTransfer('utp_abc');

    assert.equal(res.id, 'utp_abc');
    assert.equal(res.state, 'TRANSFERRING');
    assert.equal(res.size, 104857600);
    assert.equal(res.bytes_transferred, 52428800);
  });

  test('listTransfers() returns transfer array', async () => {
    const mockFetch = async () => ({
      ok: true,
      status: 200,
      text: async () =>
        JSON.stringify([
          {
            id: 'utp_1',
            source: 'file:///a',
            destination: 'file:///b',
            state: 'COMPLETED',
            size: 100,
            bytes_transferred: 100,
          },
        ]),
    });

    const client = new UTPClient({ fetchFn: mockFetch });
    const list = await client.listTransfers();
    assert.equal(list.length, 1);
    assert.equal(list[0].id, 'utp_1');
  });

  test('pauseTransfer() and cancelTransfer() issue control signals', async () => {
    let paused = false;
    let cancelled = false;

    const mockFetch = async (url) => {
      if (url.toString().endsWith('/pause')) paused = true;
      if (url.toString().endsWith('/cancel')) cancelled = true;
      return {
        ok: true,
        status: 202,
        text: async () => JSON.stringify({ status: 'requested' }),
      };
    };

    const client = new UTPClient({ fetchFn: mockFetch });
    await client.pauseTransfer('utp_ctrl');
    assert.ok(paused);

    await client.cancelTransfer('utp_ctrl');
    assert.ok(cancelled);
  });

  test('handles 401, 404, 409 HTTP errors with typed exceptions', async () => {
    const client = new UTPClient({
      fetchFn: async (url) => {
        const u = url.toString();
        if (u.includes('auth_fail')) {
          return { ok: false, status: 401, text: async () => '{"error": "Unauthorized"}' };
        }
        if (u.includes('missing')) {
          return { ok: false, status: 404, text: async () => '{"error": "Transfer not found"}' };
        }
        if (u.includes('conflict')) {
          return { ok: false, status: 409, text: async () => '{"error": "Terminal state"}' };
        }
        return { ok: true, status: 200, text: async () => '{}' };
      },
    });

    await assert.rejects(async () => client.getTransfer('auth_fail'), UTPAuthError);
    await assert.rejects(async () => client.getTransfer('missing'), UTPNotFoundError);
    await assert.rejects(async () => client.pauseTransfer('conflict'), UTPConflictError);
  });

  test('waitForCompletion() polls until COMPLETED and triggers progress callback', async () => {
    let polls = 0;
    const progressUpdates = [];

    const mockFetch = async (url) => {
      const u = url.toString();
      if (u.includes('/events')) {
        return {
          ok: true,
          status: 200,
          text: async () =>
            JSON.stringify([
              {
                transfer_id: 'utp_poll',
                type: 'TRANSFER_STARTED',
                payload: '100 bytes',
                timestamp: 1000,
              },
            ]),
        };
      }

      polls++;
      if (polls === 1) {
        return {
          ok: true,
          status: 200,
          text: async () =>
            JSON.stringify({
              id: 'utp_poll',
              source: 'file:///a',
              destination: 'file:///b',
              state: 'TRANSFERRING',
              size: 100,
              bytes_transferred: 40,
            }),
        };
      }
      return {
        ok: true,
        status: 200,
        text: async () =>
          JSON.stringify({
            id: 'utp_poll',
            source: 'file:///a',
            destination: 'file:///b',
            state: 'COMPLETED',
            size: 100,
            bytes_transferred: 100,
          }),
      };
    };

    const client = new UTPClient({ fetchFn: mockFetch });
    const result = await client.waitForCompletion('utp_poll', {
      pollIntervalMs: 10,
      onProgress: (t) => progressUpdates.push(t.bytes_transferred),
    });

    assert.equal(result.state, 'COMPLETED');
    assert.deepEqual(progressUpdates, [40, 100]);
  });

  test('waitForCompletion() throws UTPTransferFailedError on FAILED transfer', async () => {
    const mockFetch = async () => ({
      ok: true,
      status: 200,
      text: async () =>
        JSON.stringify({
          id: 'utp_fail',
          source: 'file:///a',
          destination: 'file:///b',
          state: 'FAILED',
          size: 100,
          bytes_transferred: 20,
        }),
    });

    const client = new UTPClient({ fetchFn: mockFetch });
    await assert.rejects(
      async () => client.waitForCompletion('utp_fail', { pollIntervalMs: 10 }),
      UTPTransferFailedError
    );
  });

  test('streamEvents() yields async lifecycle events', async () => {
    let call = 0;
    const mockFetch = async (url) => {
      const u = url.toString();
      if (u.includes('/events')) {
        call++;
        if (call === 1) {
          return {
            ok: true,
            status: 200,
            text: async () =>
              JSON.stringify([
                { transfer_id: 'utp_stream', type: 'TRANSFER_CREATED', payload: 'Init', timestamp: 1 },
              ]),
          };
        }
        return {
          ok: true,
          status: 200,
          text: async () =>
            JSON.stringify([
              { transfer_id: 'utp_stream', type: 'TRANSFER_CREATED', payload: 'Init', timestamp: 1 },
              { transfer_id: 'utp_stream', type: 'TRANSFER_COMPLETED', payload: 'Done', timestamp: 2 },
            ]),
        };
      }

      // /transfers/:id
      return {
        ok: true,
        status: 200,
        text: async () =>
          JSON.stringify({
            id: 'utp_stream',
            source: 'a',
            destination: 'b',
            state: call >= 2 ? 'COMPLETED' : 'TRANSFERRING',
            size: 10,
            bytes_transferred: 10,
          }),
      };
    };

    const client = new UTPClient({ fetchFn: mockFetch });
    const events = [];
    for await (const ev of client.streamEvents('utp_stream', { pollIntervalMs: 10 })) {
      events.push(ev.type);
    }

    assert.deepEqual(events, ['TRANSFER_CREATED', 'TRANSFER_COMPLETED']);
  });

  test('getStreamUrl() generates valid signed/tokenized streaming URL', () => {
    const client = new UTPClient({ baseUrl: 'http://localhost:8080', token: 'my-tok-123' });
    const streamUrl = client.getStreamUrl('gdrive://movies/clip.mp4');
    assert.equal(streamUrl, 'http://localhost:8080/api/stream?uri=gdrive%3A%2F%2Fmovies%2Fclip.mp4&token=my-tok-123');

    const downloadUrl = client.getStreamUrl('utp_transfer_999', { download: true, filename: 'my_video.mp4' });
    assert.equal(downloadUrl, 'http://localhost:8080/api/stream?id=utp_transfer_999&token=my-tok-123&download=true&name=my_video.mp4');
  });

  test('getSignedStreamUrl() exchanges Bearer token for short-lived ticket', async () => {
    let seenAuth = '';
    const mockFetch = async (_url, init) => {
      seenAuth = init?.headers?.Authorization || '';
      return {
        ok: true,
        status: 200,
        text: async () => JSON.stringify({ ticket: '1900000000.nonce.sig', expires_in: 300 }),
      };
    };

    const client = new UTPClient({ baseUrl: 'http://127.0.0.1:8080', token: 'master-secret-xyz', fetchFn: mockFetch });
    const signedUrl = await client.getSignedStreamUrl('gdrive://movies/clip.mp4', { download: true, filename: 'clip.mp4' });
    assert.equal(seenAuth, 'Bearer master-secret-xyz');
    assert.ok(signedUrl.includes('ticket=1900000000.nonce.sig'));
    assert.ok(!signedUrl.includes('master-secret-xyz'));
    assert.ok(signedUrl.includes('download=true'));
  });
});
