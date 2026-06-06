/**
 * FlyflyUFS Explorer — Mock API Layer
 * Simulates the C++ filesystem backend via the WebSocket protocol.
 * Maintains internal state and provides the same command interface as ws-client.js.
 */

const MockAPI = (function () {
  'use strict';

  // ── Internal State ────────────────────────────────────────────────────────

  let requestId = 0;
  let currentPath = '/';
  let loggedIn = false;
  let currentUser = null;

  // ── Realistic File System Tree ────────────────────────────────────────────

  // The tree mirrors a Unix-like filesystem structure stored in data.img.
  // Inode addresses are realistic offsets based on const.h layout:
  //   INODE_START_ADDR = 11776, each inode = 128 bytes, 4 inodes per 512B block.
  // We allocate sequential inode addresses for tree building.
  let nextInode = 11776;

  function allocInode() {
    const addr = nextInode;
    nextInode += 128;
    return addr;
  }

  function now() {
    return new Date().toISOString();
  }

  // File tree is a nested object keyed by full path.
  // Each entry: { name, type, size, permissions, inodeAddr, owner, group, mtime, children? (for dirs, Map of name->entry), content? (for files) }
  let fsTree = {};

  function makeDir(name, owner = 'root', group = 'root', perm = '0755') {
    return {
      name,
      type: 'dir',
      size: 512,           // directory metadata block
      permissions: perm,
      inodeAddr: allocInode(),
      owner,
      group,
      mtime: now(),
      children: new Map(),
    };
  }

  function makeFile(name, size, content = '', owner = 'root', group = 'root', perm = '0644') {
    return {
      name,
      type: 'file',
      size,
      permissions: perm,
      inodeAddr: allocInode(),
      owner,
      group,
      mtime: now(),
      content,
    };
  }

  function getDir(path) {
    if (path === '/' || path === '') return fsTree['/'] || null;
    const parts = path.replace(/^\/+|\/+$/g, '').split('/');
    let current = fsTree['/'];
    if (!current) return null;
    for (const part of parts) {
      if (!current.children || !current.children.has(part)) return null;
      current = current.children.get(part);
      if (current.type !== 'dir') return null;
    }
    return current;
  }

  function getEntry(path) {
    const parentPath = Helpers.parentPath(path);
    const name = Helpers.basename(path);
    const parent = getDir(parentPath);
    if (!parent || !parent.children) return null;
    return parent.children.get(name) || null;
  }

  function buildFileInfo(entry) {
    return {
      name: entry.name,
      type: entry.type,
      size: entry.type === 'dir' ? computeDirSize(entry) : entry.size,
      permissions: entry.permissions,
      inodeAddr: entry.inodeAddr,
      owner: entry.owner,
      group: entry.group,
    };
  }

  function computeDirSize(dir) {
    let total = 512; // directory itself
    if (dir.children) {
      for (const child of dir.children.values()) {
        if (child.type === 'dir') {
          total += computeDirSize(child);
        } else {
          total += child.size;
        }
      }
    }
    return total;
  }

  function countDirChildren(dir) {
    return dir.children ? dir.children.size : 0;
  }

  // ── Build Sample Filesystem ───────────────────────────────────────────────

  function buildSampleFS() {
    const root = makeDir('/', 'root', 'root', '0755');
    fsTree['/'] = root;

    const home = makeDir('home', 'root', 'root', '0755');
    root.children.set('home', home);

    const rootHome = makeDir('root', 'root', 'root', '0660');
    home.children.set('root', rootHome);

    const etc = makeDir('etc', 'root', 'root', '0755');
    root.children.set('etc', etc);
    etc.children.set('user', makeFile('user', 11, 'root:x:0:0\n', 'root', 'root', '0664'));
    etc.children.set('passwd', makeFile('passwd', 10, 'root:root\n', 'root', 'root', '0660'));
    etc.children.set('group', makeFile('group', 22, 'root::0:root\nuser::1:\n', 'root', 'root', '0664'));
  }

  // Initialize
  buildSampleFS();

  // ── Command Handlers ──────────────────────────────────────────────────────

  function handleLogin(params) {
    const { username, password } = params;
    if (!username || !password) return error('Username and password cannot be empty', 1002);

    // Mock mode accepts non-empty credentials only.
    loggedIn = true;
    currentUser = { username, group: username === 'root' ? 'root' : 'users' };
    return { username: currentUser.username, group: currentUser.group };
  }

  function handleLs(params) {
    // Support optional path parameter for listing without changing directory
    const targetPath = (params && params.path) ? params.path : currentPath;
    const dir = getDir(targetPath);
    if (!dir) return error('Directory not found: ' + targetPath, 1003);
    const items = [];
    if (dir.children) {
      for (const entry of dir.children.values()) {
        items.push(buildFileInfo(entry));
      }
    }
    return items;
  }

  function handleCd(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);

    // Handle special paths
    if (name === '.') return { path: currentPath };

    if (name === '/') {
      currentPath = '/';
      return { path: '/' };
    }

    if (name === '..') {
      if (currentPath === '/') return { path: '/' };
      const parts = currentPath.replace(/\/+$/, '').split('/');
      parts.pop();
      currentPath = parts.join('/') || '/';
      return { path: currentPath };
    }

    // Absolute path support (starts with /)
    if (name.startsWith('/')) {
      const targetDir = getDir(name);
      if (!targetDir || targetDir.type !== 'dir') return error(`Directory not found: ${name}`, 1003);
      currentPath = Helpers.normalizePath(name);
      return { path: currentPath };
    }

    // Relative path: look up in current directory
    const dir = getDir(currentPath);
    if (!dir || !dir.children) return error('Current directory not found', 1003);

    const entry = dir.children.get(name);
    if (!entry) return error(`Directory not found: ${name}`, 1003);
    if (entry.type !== 'dir') return error(`Not a directory: ${name}`, 1002);

    currentPath = Helpers.normalizePath(currentPath + '/' + name);
    return { path: currentPath };
  }

  function handleMkdir(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);
    if (name.length > 28) return error('Name too long (max 28 chars)', 1007);

    const dir = getDir(currentPath);
    if (!dir) return error('Current directory not found', 1003);
    if (dir.children && dir.children.has(name)) return error(`Already exists: ${name}`, 1004);

    if (!dir.children) dir.children = new Map();
    const newDir = makeDir(name, currentUser ? currentUser.username : 'root',
                           currentUser ? currentUser.group : 'root');
    dir.children.set(name, newDir);
    dir.mtime = now();
    return null;
  }

  function handleRmdir(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);

    const dir = getDir(currentPath);
    if (!dir || !dir.children) return error('Current directory not found', 1003);

    const entry = dir.children.get(name);
    if (!entry) return error(`Directory not found: ${name}`, 1003);
    if (entry.type !== 'dir') return error(`Not a directory: ${name}`, 1002);
    if (entry.children && entry.children.size > 0) return error('Directory not empty', 1008);

    dir.children.delete(name);
    dir.mtime = now();
    return null;
  }

  function handleTouch(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);
    if (name.length > 28) return error('Name too long (max 28 chars)', 1007);

    const dir = getDir(currentPath);
    if (!dir) return error('Current directory not found', 1003);
    if (dir.children && dir.children.has(name)) return error(`Already exists: ${name}`, 1004);

    if (!dir.children) dir.children = new Map();
    const newFile = makeFile(name, 0, '', currentUser ? currentUser.username : 'root',
                              currentUser ? currentUser.group : 'root');
    dir.children.set(name, newFile);
    dir.mtime = now();
    return null;
  }

  function handleRm(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);

    const dir = getDir(currentPath);
    if (!dir || !dir.children) return error('Current directory not found', 1003);

    const entry = dir.children.get(name);
    if (!entry) return error(`File not found: ${name}`, 1003);
    if (entry.type !== 'file') return error(`Not a file: ${name}`, 1002);

    dir.children.delete(name);
    dir.mtime = now();
    return null;
  }

  function handleCat(params) {
    const { name } = params;
    if (!name) return error('Missing parameter: name', 1002);

    const dir = getDir(currentPath);
    if (!dir || !dir.children) return error('Current directory not found', 1003);

    const entry = dir.children.get(name);
    if (!entry) return error(`File not found: ${name}`, 1003);
    if (entry.type !== 'file') return error(`Not a file: ${name}`, 1002);

    return { content: entry.content || '' };
  }

  function handleChmod(params) {
    const { name, mode } = params;
    if (!name) return error('Missing parameter: name', 1002);
    if (!mode) return error('Missing parameter: mode', 1002);

    const dir = getDir(currentPath);
    if (!dir || !dir.children) return error('Current directory not found', 1003);

    const entry = dir.children.get(name);
    if (!entry) return error(`Not found: ${name}`, 1003);

    entry.permissions = mode;
    entry.mtime = now();
    return null;
  }

  function handlePwd() {
    return { path: currentPath };
  }

  function handleUseradd(params) {
    const { username } = params;
    if (!username) return error('Missing parameter: username', 1002);
    return null;
  }

  function handleUserdel(params) {
    const { username } = params;
    if (!username) return error('Missing parameter: username', 1002);
    return null;
  }

  function handleDiskUsage() {
    // Compute actual usage from the tree
    let totalInodes = 640;
    let usedInodes = 0;
    let totalBlocks = 10240;
    let usedBlocks = 0;

    function walk(dir) {
      usedInodes++;
      usedBlocks++; // directory metadata takes 1 block
      if (dir.children) {
        for (const entry of dir.children.values()) {
          usedInodes++;
          if (entry.type === 'dir') {
            walk(entry);
          } else {
            // File blocks: ceil(size / 512)
            const blocks = Math.ceil(entry.size / 512);
            usedBlocks += blocks;
          }
        }
      }
    }

    if (fsTree['/']) walk(fsTree['/']);
    // Add some overhead for bitmaps, superblock
    usedBlocks += 23; // superblock (1) + inode bitmap (2) + block bitmap (20)

    return {
      totalBlocks,
      usedBlocks: Math.min(usedBlocks, totalBlocks),
      freeBlocks: Math.max(0, totalBlocks - usedBlocks),
      totalInodes,
      usedInodes: Math.min(usedInodes, totalInodes),
      freeInodes: Math.max(0, totalInodes - usedInodes),
      blockSize: 512,
    };
  }

  function handleDirectorySizes() {
    const dir = getDir(currentPath);
    if (!dir || !dir.children) return [];

    const result = [];
    for (const entry of dir.children.values()) {
      if (entry.type === 'dir') {
        result.push({
          name: entry.name,
          size: computeDirSize(entry),
          childCount: countDirChildren(entry),
        });
      }
    }
    return result;
  }

  // ── Error helper ──────────────────────────────────────────────────────────

  function error(message, code = 500) {
    return { _error: true, code, message };
  }

  // ── Main dispatch ─────────────────────────────────────────────────────────

  const handlers = {
    login:           handleLogin,
    ls:              handleLs,
    cd:              handleCd,
    mkdir:           handleMkdir,
    rmdir:           handleRmdir,
    touch:           handleTouch,
    rm:              handleRm,
    cat:             handleCat,
    chmod:           handleChmod,
    pwd:             handlePwd,
    useradd:         handleUseradd,
    userdel:         handleUserdel,
    disk_usage:      handleDiskUsage,
    directory_sizes: handleDirectorySizes,
  };

  function execute(command, params = {}) {
    const id = ++requestId;

    return new Promise((resolve) => {
      // Simulate network latency (50-150ms)
      const delay = 50 + Math.random() * 100;

      setTimeout(() => {
        // Check auth (all except login require auth)
        if (command !== 'login' && !loggedIn) {
          resolve({
            type: 'error',
            id,
            code: 401,
            message: 'Not logged in. Please login first.',
          });
          return;
        }

        const handler = handlers[command];
        if (!handler) {
          resolve({
            type: 'error',
            id,
            code: 1001,
            message: `Unknown command: ${command}`,
          });
          return;
        }

        try {
          const result = handler(params);

          if (result && result._error) {
            resolve({
              type: 'error',
              id,
              code: result.code,
              message: result.message,
            });
          } else {
            resolve({
              type: 'response',
              id,
              status: 200,
              data: result,
            });
          }
        } catch (e) {
          resolve({
            type: 'error',
            id,
            code: 500,
            message: `Internal error: ${e.message}`,
          });
        }
      }, delay);
    });
  }

  // ── Public API (same interface as ws-client.js) ───────────────────────────

  return {
    /** Connect to mock API (no-op, always succeeds) */
    connect() {
      return Promise.resolve();
    },

    /** Disconnect (no-op) */
    disconnect() {
      loggedIn = false;
      currentUser = null;
    },

    /** Execute a command. Returns Promise<WsResponse|WsError> */
    send(command, params = {}) {
      return execute(command, params);
    },

    /** Convenience: login */
    async login(username = 'root', password = '123456') {
      const result = await execute('login', { username, password });
      if (result.type === 'response') {
        loggedIn = true;
        currentUser = { username, group: result.data.group };
      }
      return result;
    },

    /** Convenience: list directory */
    async ls() {
      return execute('ls', {});
    },

    /** Convenience: change directory */
    async cd(name) {
      const result = await execute('cd', { name });
      if (result.type === 'response') {
        currentPath = result.data.path;
      }
      return result;
    },

    /** Convenience: get current path */
    getCurrentPath() {
      return currentPath;
    },

    /** Check if logged in */
    isLoggedIn() {
      return loggedIn;
    },

    /** Get current user */
    getCurrentUser() {
      return currentUser;
    },

    /** Reset to initial state (for testing) */
    reset() {
      requestId = 0;
      currentPath = '/';
      loggedIn = false;
      currentUser = null;
      fsTree = {};
      nextInode = 11776;
      buildSampleFS();
    },
  };
})();

window.MockAPI = MockAPI;
