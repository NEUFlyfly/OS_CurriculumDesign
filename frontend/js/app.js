/**
 * FlyflyUFS Explorer — Main Application
 * State management, component coordination, keyboard shortcuts, CRUD operations.
 * Links Toolbar, Sidebar, ContentArea, StatusBar, ContextMenu, Toast.
 */
const App = (function () {
  'use strict';

  // ── State ─────────────────────────────────────────────────────────────────

  let api = null;           // active API (WSClient only)
  let currentPath = '/';
  let navigationHistory = [];
  let historyIndex = -1;
  let currentItems = [];
  let currentView = 'files';
  let currentUsername = '';
  let isInitialized = false;
  let isLoggedIn = false;
  let clipboard = null;
  const AUTH_SESSION_KEY = 'flyflyufs.sessionCredentials';
  const MAX_UPLOAD_SIZE = 10 * 512;
  const MAX_EDIT_SAVE_SIZE = 5120;

  // ── Initialization ────────────────────────────────────────────────────────

  async function init() {
    if (isInitialized) return;

    // Initialize components
    Toolbar.init(document.getElementById('toolbar'), {
      onNavigateBack: navigateBack,
      onNavigateForward: navigateForward,
      onNavigateUp: navigateUp,
      onRefresh: refreshCurrent,
      onNavigateTo: navigateToPath,
      onSearch: handleSearch,
      onNewFolder: createNewFolder,
      onNewFile: createNewFile,
      onRename: renameSelected,
      onDelete: deleteSelected,
      onProperties: showProperties,
    });

    Sidebar.init(document.getElementById('sidebar'), {
      onNavigate: navigateToPath,
      onTerminal: showTerminalView,
      onStorageView: showStorageView,
    });

    ContentArea.init(document.getElementById('content-area'), {
      onSelectionChange: handleSelectionChange,
      onOpenFolder: openFolder,
      onOpenFile: openFile,
      onContextMenu: showContextMenu,
      onRenameItem: renameItem,
      onCreateFolder: createFolder,
      onCreateFile: createFile,
    });

    StatusBar.init(document.getElementById('statusbar'));
    Terminal.init(document.getElementById('content-area'), {
      onCommand: executeTerminalCommand,
      getPath: () => currentPath,
      getUsername: () => currentUsername,
    });
    StorageView.init(document.getElementById('content-area'), {
      fetchSnapshot: fetchStorageSnapshot,
      fetchGroupedStack: fetchGroupedLinkingStack,
    });
    bindShutdownButton();
    bindContentContextMenuFallback();

    // Bind global keyboard shortcuts
    bindKeyboardShortcuts();

    await restoreSessionOrShowLogin();

    isInitialized = true;
  }

  // ── Terminal ──────────────────────────────────────────────────────────────

  function showTerminalView() {
    currentView = 'terminal';
    Sidebar.setActivePath('terminal');
    Terminal.show();
  }

  function showStorageView(viewKey) {
    currentView = viewKey;
    Sidebar.setActivePath(viewKey);
    Toolbar.setAddress(storageViewTitle(viewKey));
    StatusBar.update({ folderCount: 0, fileCount: 0, selectedCount: 0 });
    StorageView.show(viewKey);
  }

  async function fetchStorageSnapshot() {
    if (!isLoggedIn || !api) {
      throw new Error('请先登录');
    }

    const result = await api.send('storage_inspection', {});
    if (result.type === 'error') {
      throw new Error(result.message || '读取存储结构失败');
    }
    if (result.data && result.data.error) {
      throw new Error(result.data.error);
    }
    if (!result.data) {
      throw new Error('后端没有返回存储结构数据');
    }
    return result.data;
  }

  async function fetchGroupedLinkingStack(group) {
    if (!isLoggedIn || !api) {
      throw new Error('请先登录');
    }

    const result = await api.send('grouped_linking_stack', {
      source: group.source || '',
      groupBlockAddress: Number(group.groupBlockAddress),
    });
    if (result.type === 'error') {
      throw new Error(result.message || '读取 s_free[] 失败');
    }
    if (result.data && result.data.error) {
      throw new Error(result.data.error);
    }
    if (!result.data || !Array.isArray(result.data.stack)) {
      throw new Error('后端没有返回 s_free[] 数据');
    }
    return result.data.stack;
  }

  function invalidateStorageCache() {
    if (typeof StorageView.invalidateCache === 'function') {
      StorageView.invalidateCache();
    }
  }

  function isSuccessfulResponse(result) {
    return result
      && result.type !== 'error'
      && !(result.data && result.data.error);
  }

  function isStorageView(viewKey) {
    return typeof viewKey === 'string' && viewKey.startsWith('storage-');
  }

  function storageViewTitle(viewKey) {
    const labels = {
      'storage-whole-disk': '整盘块布局',
      'storage-metadata-layout': '元数据区详情',
      'storage-disk-blocks': '磁盘块情况',
      'storage-inode-table': 'inode 表',
      'storage-inode-bitmap': 'inode 位图',
      'storage-block-bitmap': 'block 位图',
      'storage-grouped-linking': '成组链接法示意图',
    };
    return labels[viewKey] || '存储结构';
  }

  function bindShutdownButton() {
    const button = document.getElementById('shutdown-button');
    if (!button) return;
    button.addEventListener('click', handleShutdown);
  }

  function bindContentContextMenuFallback() {
    document.addEventListener('contextmenu', (e) => {
      if (currentView !== 'files') return;

      const content = document.getElementById('content-area');
      if (!content || !content.contains(e.target)) return;

      e.preventDefault();
      e.stopPropagation();

      const itemEl = e.target.closest('.file-item, .list-row');
      if (itemEl && content.contains(itemEl)) {
        const name = itemEl.dataset.name;
        const type = itemEl.dataset.type;
        if (typeof ContentArea.selectOnly === 'function') {
          ContentArea.selectOnly(name);
        }
        showContextMenu(e.clientX, e.clientY, name, type);
        return;
      }

      showContextMenu(e.clientX, e.clientY, null, null);
    }, true);
  }

  async function handleShutdown() {
    const button = document.getElementById('shutdown-button');
    if (!confirm('确定要关闭 FlyflyUFS 服务端吗？')) return;

    if (button) {
      button.disabled = true;
      button.textContent = '关闭中...';
    }

    try {
      if (!WSClient.isConnected()) {
        await WSClient.connect();
      }

      const result = await WSClient.send('shutdown', {});
      if (result.type === 'error') {
        throw new Error(result.message || '关机请求失败');
      }
      if (result.data && result.data.error) {
        throw new Error(result.data.error);
      }

      clearSavedCredentials();
      isLoggedIn = false;
      api = null;
      currentUsername = '';
      currentView = 'shutdown';
      Toolbar.setAddress('服务端已请求关闭');
      StatusBar.update({ folderCount: 0, fileCount: 0, selectedCount: 0 });
      const content = document.getElementById('content-area');
      if (content) {
        content.innerHTML = `
          <div class="shutdown-state">
            <div class="shutdown-state-title">服务端正在关闭</div>
            <div class="shutdown-state-description">已经收到后端确认，WebSocket 将断开，程序将退出。</div>
          </div>
        `;
      }
      Toast.info('服务端正在关闭...');
      setTimeout(() => WSClient.disconnect(), 200);
    } catch (e) {
      const message = e && e.message ? e.message : '关机请求失败';
      Toast.error(message);
      if (button) {
        button.disabled = false;
        button.textContent = '关机';
      }
    }
  }

  async function executeTerminalCommand(commandLine) {
    if (!isLoggedIn || !api) {
      return 'Not logged in. Please login first.';
    }

    const args = parseTerminalArgs(commandLine);
    const command = (args.shift() || '').toLowerCase();

    switch (command) {
      case 'help':
        return [
          'Supported commands:',
          '  login <username> [password]',
          '  logout',
          '  useradd <username> [password]',
          '  userdel <username>',
          '  exit',
          '  cat <filename>',
          '  rm <filename>',
          '  mkdir <directoryName>',
          '  rmdir <directoryName>',
          '  cd <directoryName>',
          '  ls',
          '  pwd',
          '  chmod <filename> <permissions>',
          '  clear',
          '  touch <filename>',
          '  nano <filename>',
          '  format',
        ].join('\n');

      case 'pwd':
        return currentPath;

      case 'ls':
        return formatTerminalList(await api.ls());

      case 'cd':
        return executeTerminalCd(args[0] || '/');

      case 'mkdir':
      case 'rmdir':
      case 'touch':
      case 'rm':
        return executeTerminalMutation(command, args[0]);

      case 'cat':
        return executeTerminalCat(args[0]);

      case 'chmod':
        return executeTerminalChmod(args[0], args[1]);

      case 'nano':
        return executeTerminalNano(args[0]);

      case 'clear':
        return '';

      case 'exit':
        return executeTerminalExit();

      case 'login':
        return executeTerminalLogin(args[0], args[1]);

      case 'logout':
        return executeTerminalExit();

      case 'useradd':
        return executeTerminalUserAdd(args[0], args[1]);

      case 'userdel':
        return executeTerminalUserDel(args[0]);

      case 'format':
        return executeTerminalFormat();

      default:
        return `command not found: ${command}`;
    }
  }

  function parseTerminalArgs(commandLine) {
    const matches = commandLine.match(/"[^"]*"|'[^']*'|\S+/g) || [];
    return matches.map(part => {
      if ((part.startsWith('"') && part.endsWith('"')) || (part.startsWith("'") && part.endsWith("'"))) {
        return part.slice(1, -1);
      }
      return part;
    });
  }

  async function executeTerminalCd(path) {
    const result = await api.cd(path);
    if (result.type === 'error') return result.message || 'cd failed';

    const nextPath = result.data && result.data.path ? result.data.path : path;
    await loadDirectory(nextPath, true, false);
    Terminal.updatePrompt();
    return currentPath;
  }

  async function executeTerminalMutation(command, name) {
    if (!name) return `usage: ${command} <name>`;

    const result = await api.send(command, { name });
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) invalidateStorageCache();
    await loadDirectory(currentPath, false, false);
    Terminal.updatePrompt();
    return text;
  }

  async function executeTerminalCat(name) {
    if (!name) return 'usage: cat <name>';

    const result = await api.send('cat', { name });
    if (result.type === 'error') return result.message || 'cat failed';
    if (result.data && result.data.error) return result.data.error;
    if (result.data && typeof result.data.content === 'string') return result.data.content || '(empty file)';
    return formatTerminalResponse(result);
  }

  async function executeTerminalChmod(name, mode) {
    if (!name || !mode) return 'usage: chmod <name> <mode>';

    const parsedMode = parseInt(mode, 8);
    if (Number.isNaN(parsedMode)) return 'chmod: invalid mode';

    const result = await api.send('chmod', { name, mode: parsedMode });
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) invalidateStorageCache();
    await loadDirectory(currentPath, false, false);
    Terminal.updatePrompt();
    return text;
  }

  async function executeTerminalNano(name) {
    if (!name) return 'usage: nano <filename>';

    const result = await api.send('cat', { name });
    if (result.type === 'error') return result.message || 'nano failed';
    if (result.data && result.data.error) return result.data.error;

    let content = '';
    if (result.data && typeof result.data.content === 'string') {
      content = result.data.content;
    }

    if (content.startsWith('Permission denied.')) return content;

    if (content.startsWith(`cat ${name} : No such file`)) {
      const createResult = await api.send('touch', { name });
      if (!isSuccessfulResponse(createResult)) return formatTerminalResponse(createResult);
      invalidateStorageCache();
      content = '';
    }

    showFileContentDialog(name, content);
    return `Opened ${name} in editor`;
  }

  async function executeTerminalUserAdd(username, passwordArg) {
    if (!username) return 'usage: useradd <username> [password]';

    const password = passwordArg !== undefined ? passwordArg : window.prompt(`Password for ${username}:`);
    if (password === null) return 'useradd cancelled';
    if (!password) return 'password is required';

    const result = await api.send('useradd', { username, password });
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) invalidateStorageCache();
    await loadDirectory(currentPath, false, false);
    if (text.includes('用户注册成功') && window.confirm(`User ${username} created. Switch to ${username} now?`)) {
      return switchToUser(username, password, text);
    }
    return text;
  }

  async function executeTerminalUserDel(username) {
    if (!username) return 'usage: userdel <username>';

    const result = await api.send('userdel', { username });
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) invalidateStorageCache();
    await loadDirectory(currentPath, false, false);
    return text;
  }

  async function executeTerminalExit() {
    const result = await api.send('exit', {});
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) showLoggedOutLogin();
    return text || 'Logged out';
  }

  async function executeTerminalLogin(usernameArg, passwordArg) {
    const username = usernameArg !== undefined ? usernameArg : window.prompt('Username:');
    if (username === null) return 'login cancelled';
    if (!username) return 'username is required';

    const password = passwordArg !== undefined ? passwordArg : window.prompt(`Password for ${username}:`);
    if (password === null) return 'login cancelled';
    if (!password) return 'password is required';

    return switchToUser(username, password);
  }

  async function executeTerminalFormat() {
    const confirmation = window.prompt('This will format the filesystem. Type FORMAT to continue:');
    if (confirmation !== 'FORMAT') return 'format cancelled';

    const result = await api.send('format', { confirm: 'FORMAT' });
    const text = formatTerminalResponse(result);
    if (isSuccessfulResponse(result)) {
      invalidateStorageCache();
      showLoggedOutLogin();
    }
    return text;
  }

  function showLoggedOutLogin(prefillUsername = '') {
    clearSavedCredentials();
    isLoggedIn = false;
    api = null;
    currentUsername = '';
    currentPath = '/';
    navigationHistory = [];
    historyIndex = -1;
    currentItems = [];
    currentView = 'files';

    const overlay = document.getElementById('login-overlay');
    if (overlay) overlay.remove();
    Terminal.updatePrompt();
    showLoginScreen(prefillUsername);
  }

  async function switchToUser(username, password, previousOutput = '') {
    try {
      const exitResult = await api.send('exit', {});
      if (!isSuccessfulResponse(exitResult)) {
        return `${previousOutput}\nSwitch failed: ${formatTerminalResponse(exitResult)}`;
      }

      const loginResult = await WSClient.login(username, password);
      if (!(loginResult.type === 'response' && loginResult.data && loginResult.data.success)) {
        showLoggedOutLogin(username);
        return `${previousOutput}\nSwitch failed: invalid username or password`;
      }

      saveCredentials(username, password);
      api = WSClient;
      isLoggedIn = true;
      currentUsername = loginResult.data.user || username;
      currentPath = '/';
      navigationHistory = [];
      historyIndex = -1;
      currentItems = [];

      const appEl = document.getElementById('app');
      if (appEl) appEl.style.display = '';
      const overlay = document.getElementById('login-overlay');
      if (overlay) overlay.remove();

      await loadDirectory('/', true, currentView !== 'terminal');
      Terminal.updatePrompt();
      Toast.success(`Switched to ${username}`);
      return `${previousOutput}\nSwitched to ${username}`;
    } catch (e) {
      showLoggedOutLogin(username);
      const message = e && e.message ? e.message : 'unknown error';
      return `${previousOutput}\nSwitch failed: ${message}`;
    }
  }

  function formatTerminalList(result) {
    if (result.type === 'error') return result.message || 'ls failed';
    if (!Array.isArray(result.data) || result.data.length === 0) return '(empty)';

    return result.data.map(item => {
      const type = item.type === 'dir' ? 'd' : '-';
      const permissions = formatPermissionBits(item.permissions);
      const owner = item.owner || 'root';
      const group = item.group || 'root';
      const size = typeof item.size === 'number' ? item.size : 0;
      return `${type}${permissions}\t${owner}\t${group}\t${size} B\t${item.name}`;
    }).join('\n');
  }

  function formatPermissionBits(mode) {
    if (!mode) return '---------';

    const value = typeof mode === 'string' ? parseInt(mode, 8) : mode;
    if (Number.isNaN(value)) return String(mode);

    const symbols = ['r', 'w', 'x'];
    let result = '';
    for (let bit = 8; bit >= 0; bit--) {
      result += (value & (1 << bit)) ? symbols[(8 - bit) % 3] : '-';
    }
    return result;
  }

  function formatTerminalResponse(result) {
    if (!result) return '';
    if (result.type === 'error') return result.message || 'command failed';
    if (result.data && result.data.error) return result.data.error;
    if (result.data && result.data.output) return result.data.output;
    if (result.data && result.data.path) return result.data.path;
    return 'OK';
  }

  function getUtf8ByteSize(value) {
    if (typeof TextEncoder !== 'undefined') {
      return new TextEncoder().encode(value).length;
    }
    if (typeof Blob !== 'undefined') {
      return new Blob([value]).size;
    }
    return value.length;
  }

  // ── Authentication ────────────────────────────────────────────────────────

  async function restoreSessionOrShowLogin() {
    const savedCredentials = getSavedCredentials();
    if (!savedCredentials) {
      showLoginScreen();
      return;
    }

    const appEl = document.getElementById('app');
    appEl.style.display = 'none';

    try {
      await WSClient.connect();
      const result = await WSClient.login(savedCredentials.username, savedCredentials.password);
      if (result.type === 'response' && result.data && result.data.success) {
        api = WSClient;
        isLoggedIn = true;
        currentUsername = result.data.user || savedCredentials.username;
        appEl.style.display = '';
        await loadDirectory('/');
        Terminal.updatePrompt();
        return;
      }
    } catch (e) {
      console.warn('[App] Failed to restore login session:', e);
    }

    clearSavedCredentials();
    showLoginScreen(savedCredentials.username);
  }

  function showLoginScreen(prefillUsername = '') {
    // Hide the main app, show login overlay
    const appEl = document.getElementById('app');
    appEl.style.display = 'none';

    const overlay = document.createElement('div');
    overlay.className = 'login-overlay';
    overlay.id = 'login-overlay';
    overlay.innerHTML = `
      <div class="login-card">
        <div class="login-logo">
          <svg width="48" height="48" viewBox="0 0 16 16" fill="none">
            <rect x="1" y="1" width="14" height="14" rx="2" fill="#0078D4"/>
            <path d="M4 8l2.5 2.5L11 5.5" stroke="white" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
          </svg>
        </div>
        <div class="login-title">Fly文件系统</div>
        <div class="login-subtitle" id="login-subtitle">Connecting to server...</div>
        <form id="login-form" class="login-form" style="display:none;">
          <div class="login-field">
            <label for="login-username">Username</label>
            <input type="text" id="login-username" placeholder="root" autocomplete="username" spellcheck="false">
          </div>
          <div class="login-field">
            <label for="login-password">Password</label>
            <input type="password" id="login-password" placeholder="root" autocomplete="current-password">
          </div>
          <div class="login-error" id="login-error"></div>
          <button type="submit" class="btn btn-primary login-btn" id="login-submit">Login</button>
        </form>
      </div>
    `;

    document.body.appendChild(overlay);

    // Try to connect to backend first
    tryConnectAndShowForm(prefillUsername);
  }

  async function tryConnectAndShowForm(prefillUsername = '') {
    const subtitle = document.getElementById('login-subtitle');
    const form = document.getElementById('login-form');

    try {
      if (!WSClient.isConnected()) {
        await WSClient.connect();
      }
      if (WSClient.isConnected()) {
        subtitle.textContent = 'Connected. Please login.';
        form.style.display = 'flex';
        const usernameEl = document.getElementById('login-username');
        usernameEl.value = prefillUsername;
        (prefillUsername ? document.getElementById('login-password') : usernameEl).focus();

        // Bind form submit
        form.addEventListener('submit', handleLogin);
      } else {
        subtitle.textContent = 'Cannot connect to server. Please start FlyflyUFS-server first.';
        form.style.display = 'none';
      }
    } catch (e) {
      subtitle.textContent = 'Cannot connect to server. Please start FlyflyUFS-server first.';
      form.style.display = 'none';
    }
  }

  async function handleLogin(e) {
    e.preventDefault();

    const username = document.getElementById('login-username').value.trim();
    const password = document.getElementById('login-password').value;
    const errorEl = document.getElementById('login-error');
    const submitBtn = document.getElementById('login-submit');

    errorEl.textContent = '';

    if (!username || !password) {
      errorEl.textContent = 'Username and password cannot be empty';
      return;
    }

    submitBtn.disabled = true;
    submitBtn.textContent = 'Logging in...';

    try {
      // If connection lost, try to reconnect first
      if (!WSClient.isConnected()) {
        try {
          await WSClient.connect();
        } catch (reconnErr) {
          errorEl.textContent = 'Cannot connect to server. Please start FlyflyUFS-server first.';
          submitBtn.disabled = false;
          submitBtn.textContent = 'Login';
          return;
        }
      }

      const result = await WSClient.login(username, password);
      if (result.type === 'response' && result.data && result.data.success) {
        saveCredentials(username, password);
        api = WSClient;
        isLoggedIn = true;
        currentUsername = result.data.user || username;

        // Remove login overlay, show app
        const overlay = document.getElementById('login-overlay');
        if (overlay) overlay.remove();
        const appEl = document.getElementById('app');
        appEl.style.display = '';

        // Load initial directory
        await loadDirectory('/');
        Terminal.updatePrompt();
        Toast.success('Login successful');
      } else {
        clearSavedCredentials();
        errorEl.textContent = 'Invalid username or password';
        submitBtn.disabled = false;
        submitBtn.textContent = 'Login';
      }
    } catch (e) {
      errorEl.textContent = 'Connection error: ' + (e.message || 'Unknown error');
      submitBtn.disabled = false;
      submitBtn.textContent = 'Login';
    }
  }

  function getSavedCredentials() {
    try {
      const raw = sessionStorage.getItem(AUTH_SESSION_KEY);
      if (!raw) return null;

      const credentials = JSON.parse(raw);
      if (!credentials || !credentials.username || !credentials.password) return null;
      return credentials;
    } catch (e) {
      console.warn('[App] Failed to read saved credentials:', e);
      return null;
    }
  }

  function saveCredentials(username, password) {
    try {
      sessionStorage.setItem(AUTH_SESSION_KEY, JSON.stringify({ username, password }));
    } catch (e) {
      console.warn('[App] Failed to save credentials:', e);
    }
  }

  function clearSavedCredentials() {
    try {
      sessionStorage.removeItem(AUTH_SESSION_KEY);
    } catch (e) {
      console.warn('[App] Failed to clear saved credentials:', e);
    }
  }

  // ── Directory Operations ──────────────────────────────────────────────────

  async function loadDirectory(path, addToHistory = true, showFiles = true) {
    try {
      if (showFiles) currentView = 'files';

      // Navigate to the path (use api.cd() which handles parameter format)
      const cdResult = await api.cd(path);
      if (cdResult.type === 'error') {
        Toast.error(cdResult.message || 'Cannot access directory');
        return;
      }

      // Update current path from cd response
      if (cdResult.type === 'response' && cdResult.data && cdResult.data.path) {
        currentPath = cdResult.data.path;
      } else {
        currentPath = path;
      }

      // List contents (use api.ls() which handles response parsing)
      const lsResult = await api.ls();
      if (lsResult.type === 'response' && Array.isArray(lsResult.data)) {
        currentItems = lsResult.data;
      } else if (lsResult.type === 'error') {
        Toast.error(lsResult.message || 'Failed to list directory');
        currentItems = [];
      } else {
        currentItems = [];
      }

      // Update UI
      Toolbar.setAddress(currentPath);
      if (currentView === 'files') {
        ContentArea.setItems(currentItems);
      }

      // Update sidebar active
      Sidebar.setActivePath(currentView === 'files' ? currentPath : currentView);

      // Update status bar
      const folderCount = currentItems.filter(i => i.type === 'dir').length;
      const fileCount = currentItems.filter(i => i.type === 'file').length;
      StatusBar.update({ folderCount, fileCount, selectedCount: 0 });

      // Load disk usage
      loadDiskUsage();

      // Navigation history
      if (addToHistory) {
        // Trim forward history if we navigated from a back state
        if (historyIndex < navigationHistory.length - 1) {
          navigationHistory = navigationHistory.slice(0, historyIndex + 1);
        }
        navigationHistory.push(currentPath);
        historyIndex = navigationHistory.length - 1;
        updateNavigationButtons();
      }
    } catch (e) {
      console.error('[App] Failed to load directory:', e);
      Toast.error('Failed to load directory');
    }
  }

  function refreshCurrent() {
    if (isStorageView(currentView)) {
      StorageView.refresh();
      Toast.info('Refreshed');
      return;
    }

    loadDirectory(currentPath, false);
    Toast.info('Refreshed');
  }

  async function loadDiskUsage() {
    try {
      const result = await api.send('disk_usage', {});
      if (result.type === 'response' && result.data) {
        StatusBar.setDiskUsage(result.data);
      }
    } catch (e) {
      // Silently fail for disk usage
    }
  }

  // ── Navigation ────────────────────────────────────────────────────────────

  function navigateToPath(path) {
    loadDirectory(path, true);
  }

  function navigateBack() {
    if (historyIndex > 0) {
      historyIndex--;
      loadDirectory(navigationHistory[historyIndex], false);
      updateNavigationButtons();
    }
  }

  function navigateForward() {
    if (historyIndex < navigationHistory.length - 1) {
      historyIndex++;
      loadDirectory(navigationHistory[historyIndex], false);
      updateNavigationButtons();
    }
  }

  function navigateUp() {
    const parent = Helpers.parentPath(currentPath);
    loadDirectory(parent, true);
  }

  function updateNavigationButtons() {
    Toolbar.setNavigationState(historyIndex > 0, historyIndex < navigationHistory.length - 1);
  }

  // ── Search ────────────────────────────────────────────────────────────────

  function handleSearch(query) {
    if (currentView !== 'files') return;
    ContentArea.setItems(currentItems, query);
  }

  // ── Selection ─────────────────────────────────────────────────────────────

  function handleSelectionChange(selectedNames) {
    const folderCount = currentItems.filter(i => i.type === 'dir').length;
    const fileCount = currentItems.filter(i => i.type === 'file').length;
    StatusBar.update({
      folderCount,
      fileCount,
      selectedCount: selectedNames.length,
    });
  }

  // ── File/Folder Operations ────────────────────────────────────────────────

  function openFolder(name) {
    const newPath = Helpers.normalizePath(currentPath + '/' + name);
    loadDirectory(newPath, true);
  }

  async function openFile(name) {
    try {
      const result = await api.send('cat', { name });
      if (result.type === 'response' && result.data) {
        showFileContentDialog(name, result.data.content || '');
      } else if (result.type === 'error') {
        Toast.error(result.message);
      }
    } catch (e) {
      Toast.error('Failed to open file');
    }
  }

  function createNewFolder() {
    ContentArea.startCreate('folder');
  }

  function createNewFile() {
    ContentArea.startCreate('file');
  }

  async function createFolder(name) {
    try {
      const result = await api.send('mkdir', { name });
      if (result.type === 'response') {
        invalidateStorageCache();
        Toast.success(`Folder "${name}" created`);
        await loadDirectory(currentPath, false);
      } else {
        Toast.error(result.message || 'Failed to create folder');
      }
    } catch (e) {
      Toast.error('Failed to create folder');
    }
  }

  async function createFile(name) {
    try {
      const result = await api.send('touch', { name });
      if (isSuccessfulResponse(result)) {
        invalidateStorageCache();
        Toast.success(`File "${name}" created`);
        await loadDirectory(currentPath, false);
      } else {
        Toast.error((result.data && result.data.error) || result.message || 'Failed to create file');
      }
    } catch (e) {
      Toast.error('Failed to create file');
    }
  }

  async function renameItem(oldName, newName) {
    if (oldName === newName) return;

    try {
      const entry = currentItems.find(i => i.name === oldName);
      if (!entry) {
        Toast.error('Item not found');
        return;
      }

      if (entry.type === 'dir') {
        // For directories: rmdir + mkdir (loses children)
        await api.send('rmdir', { name: oldName });
        await api.send('mkdir', { name: newName });
      } else {
        // For files: read content -> delete old -> create new -> write content
        let content = '';
        try {
          const catResult = await api.send('cat', { name: oldName });
          if (catResult.type === 'response' && catResult.data) {
            content = catResult.data.content || '';
          }
        } catch (e) { /* ignore */ }

        await api.send('rm', { name: oldName });
        await api.send('touch', { name: newName });
        // Write the content back to the new file
        if (content) {
          await api.send('write', { name: newName, content: content });
        }
      }

      invalidateStorageCache();
      Toast.success(`Renamed to "${newName}"`);
      await loadDirectory(currentPath, false);
    } catch (e) {
      Toast.error('Failed to rename');
    }
  }

  function renameSelected() {
    const selected = ContentArea.getSelectedNames();
    if (selected.length === 1) {
      ContentArea.startRename(selected[0]);
    } else if (selected.length === 0) {
      Toast.info('Select an item to rename');
    } else {
      Toast.info('Can only rename one item at a time');
    }
  }

  async function deleteSelected() {
    const selected = ContentArea.getSelectedNames();
    if (selected.length === 0) {
      Toast.info('Select items to delete');
      return;
    }

    const confirmed = await showConfirmDialog(
      'Delete Item' + (selected.length > 1 ? 's' : ''),
      `Are you sure you want to delete ${selected.length > 1 ? selected.length + ' items' : '"' + selected[0] + '"'}? This action cannot be undone.`
    );

    if (!confirmed) return;

    let successCount = 0;
    let failCount = 0;

    for (const name of selected) {
      try {
        const entry = currentItems.find(i => i.name === name);
        const command = entry && entry.type === 'dir' ? 'rmdir' : 'rm';
        const result = await api.send(command, { name });

        if (result.type === 'response') {
          successCount++;
        } else {
          failCount++;
          Toast.error(result.message || `Failed to delete "${name}"`);
        }
      } catch (e) {
        failCount++;
      }
    }

    if (successCount > 0) {
      invalidateStorageCache();
      Toast.success(`Deleted ${successCount} item${successCount > 1 ? 's' : ''}`);
    }
    await loadDirectory(currentPath, false);
  }

  function getSelectedClipboardItems() {
    return ContentArea.getSelectedItems().map(item => ({
      name: item.name,
      type: item.type,
      path: currentPath,
    }));
  }

  function itemPath(item) {
    return Helpers.normalizePath(`${item.path || currentPath}/${item.name}`);
  }

  function copySelected() {
    if (currentView !== 'files') return;
    const items = getSelectedClipboardItems();
    if (items.length === 0) {
      Toast.info('Select items to copy');
      return;
    }

    clipboard = { action: 'copy', items, sourcePath: currentPath };
    Toast.success(`Copied ${items.length} item${items.length > 1 ? 's' : ''}`);
  }

  function cutSelected() {
    if (currentView !== 'files') return;
    const items = getSelectedClipboardItems();
    if (items.length === 0) {
      Toast.info('Select items to cut');
      return;
    }

    clipboard = { action: 'cut', items, sourcePath: currentPath };
    Toast.success(`Cut ${items.length} item${items.length > 1 ? 's' : ''}`);
  }

  async function pasteInto(targetPath = currentPath) {
    if (currentView !== 'files') return;
    if (!clipboard || !clipboard.items || clipboard.items.length === 0) {
      Toast.info('Nothing to paste');
      return;
    }

    const action = clipboard.action;
    const command = action === 'cut' ? 'move' : 'copy';
    let successCount = 0;
    let failCount = 0;

    for (const item of clipboard.items) {
      try {
        const result = await api.send(command, {
          sourcePath: itemPath(item),
          targetPath,
        });
        if (result.type === 'response' && result.data && result.data.success) {
          successCount++;
        } else {
          failCount++;
          Toast.error((result.data && result.data.error) || result.message || `Failed to paste "${item.name}"`);
        }
      } catch (e) {
        failCount++;
        Toast.error(`Failed to paste "${item.name}"`);
      }
    }

    if (successCount > 0) {
      invalidateStorageCache();
      if (action === 'cut' && failCount === 0) clipboard = null;
      Toast.success(`${action === 'cut' ? 'Moved' : 'Copied'} ${successCount} item${successCount > 1 ? 's' : ''}`);
      await loadDirectory(currentPath, false);
    }
  }

  function uploadLocalFile(targetPath = currentPath) {
    if (currentView !== 'files') return;

    const input = document.createElement('input');
    input.type = 'file';
    input.style.display = 'none';
    document.body.appendChild(input);

    input.addEventListener('change', () => {
      const file = input.files && input.files[0];
      input.remove();
      if (!file) return;

      if (file.size > MAX_UPLOAD_SIZE) {
        Toast.error(`File too large. Maximum supported size is ${MAX_UPLOAD_SIZE} bytes.`);
        return;
      }

      const reader = new FileReader();
      reader.onload = async () => {
        const content = String(reader.result || '');
        if (content.includes('\0')) {
          Toast.error('Only text file upload is supported');
          return;
        }

        try {
          const result = await api.send('upload', {
            targetPath,
            name: file.name,
            content,
          });
          if (result.type === 'response' && result.data && result.data.success) {
            invalidateStorageCache();
            Toast.success(`Uploaded "${file.name}"`);
            await loadDirectory(currentPath, false);
          } else {
            Toast.error((result.data && result.data.error) || result.message || 'Upload failed');
          }
        } catch (e) {
          Toast.error('Upload failed');
        }
      };
      reader.onerror = () => Toast.error('Failed to read local file');
      reader.readAsText(file);
    }, { once: true });

    input.click();
  }

  function showProperties() {
    const selected = ContentArea.getSelectedItems();
    if (selected.length === 0) {
      Toast.info('Select an item to view properties');
      return;
    }

    // Show first selected item's properties
    const item = selected[0];
    showPropertiesForItem(item);
  }

  // ── Context Menu ──────────────────────────────────────────────────────────

  function showContextMenu(x, y, name, type) {
    let menuItems = [];

    if (name && type) {
      // Item context menu
      menuItems = [
        { label: 'Copy', icon: Icons.copy, shortcut: 'Ctrl+C', action: copySelected },
        { label: 'Cut', icon: Icons.copy, shortcut: 'Ctrl+X', action: cutSelected },
        { label: 'Delete', icon: Icons.delete, shortcut: 'Del', action: deleteSelected },
        { label: 'Properties', icon: Icons.properties, action: () => {
          const item = currentItems.find(i => i.name === name);
          if (item) showPropertiesForItem(item);
        }},
      ];
    } else {
      // Empty space context menu
      menuItems = [
        { label: 'Paste', icon: Icons.copy, shortcut: 'Ctrl+V', action: () => pasteInto(currentPath) },
        { label: 'Upload Local File', icon: Icons.newFile, action: () => uploadLocalFile(currentPath) },
        { separator: true },
        { label: 'New Folder', icon: Icons.newFolder, shortcut: 'Ctrl+Shift+N', action: () => {
          createNewFolder();
        }},
        { label: 'New File', icon: Icons.newFile, action: () => {
          createNewFile();
        }},
        { separator: true },
        { label: 'Refresh', icon: Icons.refresh, shortcut: 'F5', action: () => {
          refreshCurrent();
        }},
        { separator: true },
        { label: 'Properties', icon: Icons.properties, action: () => {
          Toast.info('Folder properties not yet implemented');
        }},
      ];
    }

    ContextMenu.show(x, y, menuItems);
  }

  // ── Dialogs ───────────────────────────────────────────────────────────────

  function showConfirmDialog(title, message) {
    return new Promise((resolve) => {
      const overlay = document.createElement('div');
      overlay.className = 'modal-overlay';
      overlay.innerHTML = `
        <div class="modal-dialog">
          <div class="modal-title">${Helpers.escapeHtml(title)}</div>
          <div class="modal-body">${Helpers.escapeHtml(message)}</div>
          <div class="modal-actions">
            <button class="btn btn-secondary" id="modal-cancel">Cancel</button>
            <button class="btn btn-danger" id="modal-confirm">Delete</button>
          </div>
        </div>
      `;

      document.body.appendChild(overlay);

      overlay.querySelector('#modal-cancel').addEventListener('click', () => {
        document.body.removeChild(overlay);
        resolve(false);
      });

      overlay.querySelector('#modal-confirm').addEventListener('click', () => {
        document.body.removeChild(overlay);
        resolve(true);
      });

      overlay.addEventListener('click', (e) => {
        if (e.target === overlay) {
          document.body.removeChild(overlay);
          resolve(false);
        }
      });

      // Focus cancel by default
      overlay.querySelector('#modal-cancel').focus();
    });
  }

  function showFileContentDialog(name, content) {
    const overlay = document.createElement('div');
    overlay.className = 'modal-overlay';
    overlay.innerHTML = `
      <div class="modal-dialog" style="max-width:720px;max-height:85vh;display:flex;flex-direction:column;">
        <div class="modal-title" style="display:flex;align-items:center;justify-content:space-between;">
          <span>${Helpers.escapeHtml(name)}</span>
          <span id="editor-status" style="font-size:var(--text-sm);font-weight:var(--weight-normal);color:var(--text-tertiary);"></span>
        </div>
        <div class="modal-body" style="flex:1;overflow:hidden;padding:0;margin-bottom:var(--space-3);">
          <textarea id="file-editor" class="file-editor-textarea" spellcheck="false">${Helpers.escapeHtml(content)}</textarea>
        </div>
        <div class="modal-actions" style="margin-top:0;margin-bottom:0;">
          <button class="btn btn-secondary" id="modal-close">Close</button>
          <button class="btn btn-primary" id="modal-save">Save</button>
        </div>
      </div>
    `;

    document.body.appendChild(overlay);

    const textarea = overlay.querySelector('#file-editor');
    const saveBtn = overlay.querySelector('#modal-save');
    const statusEl = overlay.querySelector('#editor-status');
    let modified = false;

    textarea.addEventListener('input', () => {
      if (!modified) {
        modified = true;
        statusEl.textContent = 'Modified';
        statusEl.style.color = 'var(--warning)';
      }
    });

    // Ctrl+S to save
    textarea.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 's') {
        e.preventDefault();
        saveFile();
      }
    });

    async function saveFile() {
      const newContent = textarea.value;
      const byteSize = getUtf8ByteSize(newContent);
      if (byteSize > MAX_EDIT_SAVE_SIZE) {
        const message = `Save failed: file size ${byteSize} B exceeds ${MAX_EDIT_SAVE_SIZE} B limit.`;
        Toast.error(message);
        statusEl.textContent = `${byteSize} B > ${MAX_EDIT_SAVE_SIZE} B`;
        statusEl.style.color = 'var(--error)';
        return;
      }

      saveBtn.disabled = true;
      saveBtn.textContent = 'Saving...';

      try {
        const result = await api.send('write', { name, content: newContent });
        if (result.type === 'response' && !(result.data && result.data.error)) {
          invalidateStorageCache();
          modified = false;
          statusEl.textContent = 'Saved';
          statusEl.style.color = 'var(--success)';
          Toast.success(`File "${name}" saved`);
          await loadDirectory(currentPath, false);
        } else {
          const errMsg = result.message || (result.data && result.data.error) || 'Save failed';
          Toast.error(errMsg);
          statusEl.textContent = 'Save failed';
          statusEl.style.color = 'var(--error)';
        }
      } catch (e) {
        Toast.error('Failed to save file');
        statusEl.textContent = 'Save failed';
        statusEl.style.color = 'var(--error)';
      }

      saveBtn.disabled = false;
      saveBtn.textContent = 'Save';
    }

    saveBtn.addEventListener('click', saveFile);

    const close = () => {
      if (overlay.parentNode) document.body.removeChild(overlay);
    };

    overlay.querySelector('#modal-close').addEventListener('click', close);
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) close();
    });

    textarea.focus();
  }

  async function showPropertiesForItem(item) {
    try {
      const result = await api.send('properties', { name: item.name });
      if (result.type === 'response' && result.data && result.data.success && result.data.properties) {
        showPropertiesDialog(item, result.data.properties);
        return;
      }

      const message = result.message || (result.data && result.data.error) || 'Failed to load properties';
      Toast.error(message);
    } catch (e) {
      Toast.error('Failed to load properties');
    }
  }

  function showPropertiesDialog(item, properties = {}) {
    const merged = { ...item, ...properties };
    const isDir = merged.type === 'dir';
    const size = isDir ? '--' : Helpers.formatSize(merged.size || 0);
    const typeLabel = isDir ? 'File folder' : getFileTypeLabel(merged.name || item.name);
    const permissionText = formatPropertyPermissions(merged);
    const inodeText = formatInodeValue(merged);
    const modifiedText = merged.modified ? Helpers.formatDate(merged.modified) : '--';

    const overlay = document.createElement('div');
    overlay.className = 'modal-overlay';
    overlay.innerHTML = `
      <div class="modal-dialog" style="max-width:420px;">
        <div class="modal-title">${Helpers.escapeHtml(merged.name || item.name)} Properties</div>
        <div class="modal-body">
          <div class="properties-grid">
            <div class="prop-label">Type:</div><div class="prop-value">${Helpers.escapeHtml(typeLabel)}</div>
            <div class="prop-label">Location:</div><div class="prop-value">${Helpers.escapeHtml(merged.location || currentPath)}</div>
            <div class="prop-label">Size:</div><div class="prop-value">${Helpers.escapeHtml(size)}</div>
            <div class="prop-label">Permissions:</div><div class="prop-value">${Helpers.escapeHtml(permissionText)}</div>
            <div class="prop-label">Owner:</div><div class="prop-value">${Helpers.escapeHtml(merged.owner || '--')}</div>
            <div class="prop-label">Group:</div><div class="prop-value">${Helpers.escapeHtml(merged.group || '--')}</div>
            <div class="prop-label">Inode:</div><div class="prop-value">${Helpers.escapeHtml(inodeText)}</div>
            <div class="prop-label">Modified:</div><div class="prop-value">${Helpers.escapeHtml(modifiedText)}</div>
          </div>
        </div>
        <div class="modal-actions" style="margin-top:var(--space-4);margin-bottom:0;">
          <button class="btn btn-secondary" id="modal-close">OK</button>
        </div>
      </div>
    `;

    document.body.appendChild(overlay);

    const close = () => {
      if (overlay.parentNode) document.body.removeChild(overlay);
    };

    overlay.querySelector('#modal-close').addEventListener('click', close);
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) close();
    });
  }

  function getFileTypeLabel(name) {
    const dotIndex = name.lastIndexOf('.');
    if (dotIndex > 0 && dotIndex < name.length - 1) {
      return `${name.slice(dotIndex + 1).toUpperCase()} File`;
    }
    return 'File';
  }

  function formatPropertyPermissions(properties) {
    const rwx = properties.permissions && properties.permissions.length === 9
      ? properties.permissions
      : Helpers.formatPermissions(properties.permissionOctal || properties.permissions);
    const octal = properties.permissionOctal || properties.permissions || '';
    return octal ? `${rwx} (${octal})` : rwx;
  }

  function formatInodeValue(properties) {
    const hasId = properties.inodeId !== undefined && properties.inodeId !== null;
    const hasAddress = properties.inodeAddress !== undefined && properties.inodeAddress !== null && properties.inodeAddress >= 0;
    if (hasId && hasAddress) return `${properties.inodeId} (addr ${properties.inodeAddress})`;
    if (hasId) return String(properties.inodeId);
    if (hasAddress) return String(properties.inodeAddress);
    return '--';
  }

  // ── Keyboard Shortcuts ────────────────────────────────────────────────────

  function bindKeyboardShortcuts() {
    document.addEventListener('keydown', (e) => {
      // Don't handle when typing in inputs (except Escape)
      const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
      const isInput = tag === 'input' || tag === 'textarea' || document.activeElement.isContentEditable;

      if (isInput && e.key !== 'Escape') return;

      if ((e.ctrlKey || e.metaKey) && currentView === 'files') {
        const key = e.key.toLowerCase();
        if (key === 'c') {
          e.preventDefault();
          copySelected();
          return;
        }
        if (key === 'x') {
          e.preventDefault();
          cutSelected();
          return;
        }
        if (key === 'v') {
          e.preventDefault();
          pasteInto(currentPath);
          return;
        }
      }

      // F2 - Rename
      if (e.key === 'F2') {
        e.preventDefault();
        renameSelected();
        return;
      }

      // F5 - Refresh
      if (e.key === 'F5') {
        e.preventDefault();
        refreshCurrent();
        return;
      }

      // Delete - Delete selected
      if (e.key === 'Delete') {
        e.preventDefault();
        deleteSelected();
        return;
      }

      // Enter - Open selected
      if (e.key === 'Enter') {
        const selected = ContentArea.getSelectedItems();
        if (selected.length === 1) {
          e.preventDefault();
          if (selected[0].type === 'dir') {
            openFolder(selected[0].name);
          } else {
            openFile(selected[0].name);
          }
        }
        return;
      }

      // Escape - Clear selection, close context menu
      if (e.key === 'Escape') {
        ContentArea.clearSelection();
        ContextMenu.hide();
        return;
      }

      // Ctrl+A - Select all
      if ((e.ctrlKey || e.metaKey) && e.key === 'a') {
        e.preventDefault();
        ContentArea.selectAll();
        return;
      }

      // Backspace - Navigate up
      if (e.key === 'Backspace' && !isInput) {
        e.preventDefault();
        navigateUp();
        return;
      }

      // Alt+Left - Back
      if (e.altKey && e.key === 'ArrowLeft') {
        e.preventDefault();
        navigateBack();
        return;
      }

      // Alt+Right - Forward
      if (e.altKey && e.key === 'ArrowRight') {
        e.preventDefault();
        navigateForward();
        return;
      }

      // Ctrl+L - Focus address bar
      if ((e.ctrlKey || e.metaKey) && e.key === 'l') {
        e.preventDefault();
        Toolbar.focusAddressBar();
        return;
      }
    });
  }

  // ── Public API ────────────────────────────────────────────────────────────

  return {
    init,
    getAPI() { return api; },
    refreshCurrent,
  };
})();

// Auto-init when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  App.init().catch(err => {
    console.error('[App] Initialization failed:', err);
  });
});

window.App = App;
