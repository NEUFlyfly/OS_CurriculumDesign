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
  let isInitialized = false;
  let isLoggedIn = false;

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

    // Bind global keyboard shortcuts
    bindKeyboardShortcuts();

    // Show login screen
    showLoginScreen();

    isInitialized = true;
  }

  // ── Authentication ────────────────────────────────────────────────────────

  function showLoginScreen() {
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
    tryConnectAndShowForm();
  }

  async function tryConnectAndShowForm() {
    const subtitle = document.getElementById('login-subtitle');
    const form = document.getElementById('login-form');

    try {
      await WSClient.connect();
      if (WSClient.isConnected()) {
        subtitle.textContent = 'Connected. Please login.';
        form.style.display = 'flex';
        document.getElementById('login-username').focus();

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

    const username = document.getElementById('login-username').value.trim() || 'root';
    const password = document.getElementById('login-password').value || 'root';
    const errorEl = document.getElementById('login-error');
    const submitBtn = document.getElementById('login-submit');

    errorEl.textContent = '';
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
        api = WSClient;
        isLoggedIn = true;

        // Remove login overlay, show app
        const overlay = document.getElementById('login-overlay');
        if (overlay) overlay.remove();
        const appEl = document.getElementById('app');
        appEl.style.display = '';

        // Load initial directory
        await loadDirectory('/');
        Toast.success('Login successful');
      } else {
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

  // ── Directory Operations ──────────────────────────────────────────────────

  async function loadDirectory(path, addToHistory = true) {
    try {
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
      ContentArea.setItems(currentItems);

      // Update sidebar active
      Sidebar.setActivePath(currentPath);

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
      if (result.type === 'response') {
        Toast.success(`File "${name}" created`);
        await loadDirectory(currentPath, false);
      } else {
        Toast.error(result.message || 'Failed to create file');
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
      Toast.success(`Deleted ${successCount} item${successCount > 1 ? 's' : ''}`);
    }
    await loadDirectory(currentPath, false);
  }

  function showProperties() {
    const selected = ContentArea.getSelectedItems();
    if (selected.length === 0) {
      Toast.info('Select an item to view properties');
      return;
    }

    // Show first selected item's properties
    const item = selected[0];
    showPropertiesDialog(item);
  }

  // ── Context Menu ──────────────────────────────────────────────────────────

  function showContextMenu(x, y, name, type) {
    let menuItems = [];

    if (name && type) {
      // Item context menu
      menuItems = [
        { label: 'Open', icon: Icons.open, shortcut: 'Enter', action: () => {
          if (type === 'dir') openFolder(name);
          else openFile(name);
        }},
        { separator: true },
        { label: 'Rename', icon: Icons.rename, shortcut: 'F2', action: () => {
          ContentArea.startRename(name);
        }},
        { label: 'Delete', icon: Icons.delete, shortcut: 'Del', action: () => {
          deleteSelected();
        }},
        { separator: true },
        { label: 'Properties', icon: Icons.properties, action: () => {
          const item = currentItems.find(i => i.name === name);
          if (item) showPropertiesDialog(item);
        }},
      ];
    } else {
      // Empty space context menu
      menuItems = [
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
      saveBtn.disabled = true;
      saveBtn.textContent = 'Saving...';

      try {
        const result = await api.send('write', { name, content: newContent });
        if (result.type === 'response' || (result.data && !result.data.error)) {
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

  function showPropertiesDialog(item) {
    const size = item.type === 'dir' ? '--' : Helpers.formatSize(item.size);
    const typeLabel = item.type === 'dir' ? 'File folder' : (item.name.split('.').pop().toUpperCase() + ' File');

    const overlay = document.createElement('div');
    overlay.className = 'modal-overlay';
    overlay.innerHTML = `
      <div class="modal-dialog" style="max-width:420px;">
        <div class="modal-title">${Helpers.escapeHtml(item.name)} Properties</div>
        <div class="modal-body">
          <div class="properties-grid">
            <div class="prop-label">Type:</div><div class="prop-value">${typeLabel}</div>
            <div class="prop-label">Location:</div><div class="prop-value">${Helpers.escapeHtml(currentPath)}</div>
            <div class="prop-label">Size:</div><div class="prop-value">${size}</div>
            <div class="prop-label">Permissions:</div><div class="prop-value">${Helpers.formatPermissions(item.permissions)} (${item.permissions})</div>
            <div class="prop-label">Owner:</div><div class="prop-value">${Helpers.escapeHtml(item.owner || 'root')}</div>
            <div class="prop-label">Group:</div><div class="prop-value">${Helpers.escapeHtml(item.group || 'root')}</div>
            <div class="prop-label">Inode:</div><div class="prop-value">${item.inodeAddr || '--'}</div>
            <div class="prop-label">Modified:</div><div class="prop-value">${Helpers.formatDate(item.mtime)}</div>
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

  // ── Keyboard Shortcuts ────────────────────────────────────────────────────

  function bindKeyboardShortcuts() {
    document.addEventListener('keydown', (e) => {
      // Don't handle when typing in inputs (except Escape)
      const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
      const isInput = tag === 'input' || tag === 'textarea' || document.activeElement.isContentEditable;

      if (isInput && e.key !== 'Escape') return;

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
