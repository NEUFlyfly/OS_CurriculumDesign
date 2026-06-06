/**
 * FlyflyUFS Explorer — WebSocket Client
 * Connects to the C++ backend at ws://localhost:9001/ws.
 * Exports the same command interface for seamless usage.
 */

const WSClient = (function () {
  'use strict';

  // ── Configuration ─────────────────────────────────────────────────────────

  const WS_URL = 'ws://127.0.0.1:9001/ws';
  const RECONNECT_DELAY = 2000;
  const MAX_RECONNECT_ATTEMPTS = 3;

  // ── State ─────────────────────────────────────────────────────────────────

  let ws = null;
  let connected = false;
  let reconnectAttempts = 0;
  let reconnectTimer = null;
  let requestId = 0;
  let pendingRequests = new Map();  // id -> { resolve, reject, timer }
  let eventListeners = {};

  // ── Event Emitter ─────────────────────────────────────────────────────────

  function emit(event, ...args) {
    (eventListeners[event] || []).forEach(fn => {
      try { fn(...args); } catch (e) { console.error('[WSClient] Event handler error:', e); }
    });
  }

  // ── Connection Management ─────────────────────────────────────────────────

  function connect() {
    return new Promise((resolve, reject) => {
      try {
        ws = new WebSocket(WS_URL);

        ws.onopen = () => {
          console.log('[WSClient] Connected to backend');
          connected = true;
          reconnectAttempts = 0;
          emit('connected');
          resolve();
        };

        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);
            handleMessage(message);
          } catch (e) {
            console.error('[WSClient] Failed to parse message:', e);
          }
        };

        ws.onclose = (event) => {
          console.log('[WSClient] Disconnected:', event.code, event.reason);
          connected = false;
          ws = null;
          emit('disconnected', event);

          // Reject all pending requests
          pendingRequests.forEach((req, id) => {
            clearTimeout(req.timer);
            req.reject(new Error('Connection closed'));
          });
          pendingRequests.clear();

          // Auto-reconnect
          if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            scheduleReconnect();
          }
        };

        ws.onerror = (error) => {
          console.error('[WSClient] WebSocket error:', error);
          emit('error', error);
          reject(new Error('WebSocket connection failed'));
        };

      } catch (e) {
        console.warn('[WSClient] Cannot create WebSocket:', e.message);
        reject(e);
      }
    });
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectAttempts++;
    console.log(`[WSClient] Reconnecting in ${RECONNECT_DELAY}ms (attempt ${reconnectAttempts}/${MAX_RECONNECT_ATTEMPTS})`);

    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connect().catch(() => {
        // connect() will handle further retries internally
      });
    }, RECONNECT_DELAY);
  }

  function disconnect() {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    reconnectAttempts = MAX_RECONNECT_ATTEMPTS; // prevent auto-reconnect

    if (ws) {
      ws.close(1000, 'Client disconnect');
      ws = null;
    }
    connected = false;

    // Reject pending requests
    pendingRequests.forEach((req, id) => {
      clearTimeout(req.timer);
      req.reject(new Error('Disconnected'));
    });
    pendingRequests.clear();
  }

  // ── Message Handling ──────────────────────────────────────────────────────

  function handleMessage(message) {
    const { type, id } = message;

    if (type === 'response' || type === 'error') {
      // Server may return id as string or number; try both to find pending request
      const pending = pendingRequests.get(id) || pendingRequests.get(Number(id)) || pendingRequests.get(String(id));
      if (pending) {
        clearTimeout(pending.timer);
        pendingRequests.delete(id);
        pendingRequests.delete(Number(id));
        pendingRequests.delete(String(id));
        pending.resolve(message);
      }
    }

    // Also emit for broadcast handling
    emit('message', message);
  }

  // ── Send Command ──────────────────────────────────────────────────────────

  function send(command, params = {}, timeout = 30000) {
    const id = ++requestId;
    const request = JSON.stringify({ type: 'request', id, command, params });

    return new Promise((resolve, reject) => {
      if (!connected || !ws || ws.readyState !== WebSocket.OPEN) {
        reject(new Error('Not connected'));
        return;
      }

      const timer = setTimeout(() => {
        pendingRequests.delete(id);
        reject(new Error(`Request timeout: ${command}`));
      }, timeout);

      pendingRequests.set(id, { resolve, reject, timer });

      try {
        ws.send(request);
      } catch (e) {
        clearTimeout(timer);
        pendingRequests.delete(id);
        reject(e);
      }
    });
  }

  // ── Output Parsing ──────────────────────────────────────────────────────

  /**
   * Parse the text output from C++ ShowDir() into structured file entries.
   * Output format per line:
   *   permissions\tlinks\towner\tgroup\tsize B\tname
   * Example:
   *   drwxr-xr-x	3	root	root	  512 B	home
   *   drwxr-xr-x	5	root	root	  512 B	etc
   */
  function parseLsOutput(output) {
    if (!output || typeof output !== 'string') return [];

    const entries = [];
    // Handle both \r\n and \n line endings
    const lines = output.replace(/\r\n/g, '\n').trim().split('\n');

    for (const line of lines) {
      if (!line.trim()) continue;

      // Match: permissions TAB links TAB owner TAB group TAB size B TAB name
      // Size may have leading spaces: "  512 B"
      const match = line.match(/^([d\-][rwx\-]{9})\t(\d+)\t(\S+)\t(\S+)\t\s*(\d+)\s+B\t(.+)$/);
      if (match) {
        const [, permissions, links, owner, group, sizeStr, name] = match;
        const isDir = permissions.charAt(0) === 'd';
        entries.push({
          name: name.trim(),
          type: isDir ? 'dir' : 'file',
          size: parseInt(sizeStr, 10),
          permissions: permissionsToOctal(permissions),
          inodeAddr: 0,  // Not available in text output
          owner,
          group,
        });
      }
    }

    return entries;
  }

  /**
   * Convert permission string like "rwxr-xr-x" to octal string "0755".
   */
  function permissionsToOctal(perm) {
    // perm is like "drwxr-xr-x" or "-rw-r--r--"
    const bits = perm.substring(1); // Remove first char (d or -)
    let octal = 0;

    for (let i = 0; i < 9; i++) {
      if (bits.charAt(i) !== '-') {
        octal |= (1 << (8 - i));
      }
    }

    return '0' + octal.toString(8).padStart(3, '0');
  }

  // ── Convenience Methods ───────────────────────────────────────────────────

  async function login(username, password) {
    const result = await send('login', { username, password });
    if (result.type === 'response') {
      emit('login', result.data);
    }
    return result;
  }

  async function ls() {
    const result = await send('ls', {});
    // C++ backend returns { output: "text...", path: "..." }
    // Convert to structured array format expected by frontend
    if (result.type === 'response' && result.data && typeof result.data.output === 'string') {
      const entries = parseLsOutput(result.data.output);
      return {
        type: 'response',
        id: result.id,
        status: 200,
        data: entries,
      };
    }
    return result;
  }

  async function cd(name) {
    const result = await send('cd', { path: name });
    // Check for error in cd response
    if (result.type === 'response' && result.data && result.data.error) {
      return {
        type: 'error',
        message: result.data.error,
        data: result.data,
      };
    }
    return result;
  }

  function on(event, fn) {
    if (!eventListeners[event]) eventListeners[event] = [];
    eventListeners[event].push(fn);
    return () => {
      const idx = eventListeners[event].indexOf(fn);
      if (idx >= 0) eventListeners[event].splice(idx, 1);
    };
  }

  function isConnected() {
    return connected && ws && ws.readyState === WebSocket.OPEN;
  }

  function isUsingFallback() {
    return false; // Never using fallback
  }

  // ── Public API ────────────────────────────────────────────────────────────

  return {
    connect,
    disconnect,
    send,
    login,
    ls,
    cd,
    on,
    isConnected,
    isUsingFallback,
  };
})();

window.WSClient = WSClient;
