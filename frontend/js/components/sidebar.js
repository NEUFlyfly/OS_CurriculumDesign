/**
 * FlyflyUFS Explorer — Sidebar Component
 * Quick access and hierarchical folder tree navigation.
 * Emits events for App.js to handle.
 */
const Sidebar = (function () {
  'use strict';

  let container = null;
  let callbacks = {};
  let activePath = '/';
  let treeState = {}; // path -> expanded boolean
  let directoryCache = {}; // path -> array of dir entries

  // ── Quick Access Items ────────────────────────────────────────────────────

  const quickAccess = [
    { id: 'root', label: '根目录 /', icon: Icons.folder, path: '/' },
    { id: 'terminal', label: '终端', icon: Icons.terminal, action: 'terminal' },
    { id: 'whole-disk', label: '整盘块布局', icon: Icons.storage, action: 'storage-whole-disk' },
    { id: 'metadata-layout', label: '元数据区详情', icon: Icons.storage, action: 'storage-metadata-layout' },
    { id: 'disk-blocks', label: '磁盘块情况', icon: Icons.storage, action: 'storage-disk-blocks' },
    { id: 'inode-table', label: 'inode 表', icon: Icons.storage, action: 'storage-inode-table' },
    { id: 'inode-bitmap', label: 'inode 位图', icon: Icons.storage, action: 'storage-inode-bitmap' },
    { id: 'block-bitmap', label: 'block 位图', icon: Icons.storage, action: 'storage-block-bitmap' },
    { id: 'grouped-linking', label: '成组链接法', icon: Icons.storage, action: 'storage-grouped-linking' },
  ];

  // ── Initialization ────────────────────────────────────────────────────────

  function init(parentEl, cb) {
    container = parentEl;
    callbacks = cb || {};
    render();
  }

  function render() {
    container.innerHTML = '';

    // Quick Access Section
    const qaSection = document.createElement('div');
    qaSection.className = 'sidebar-section';
    qaSection.innerHTML = '<div class="sidebar-section-header">Quick Access</div>';

    quickAccess.forEach(item => {
      const div = createSidebarItem(item.label, item.icon, false);
      if (item.path) div.dataset.path = item.path;
      if (item.action) div.dataset.action = item.action;
      div.addEventListener('click', () => {
        if (item.action) {
          setActivePath(item.action);
          if (item.action === 'terminal' && callbacks.onTerminal) {
            callbacks.onTerminal();
          } else if (item.action.startsWith('storage-') && callbacks.onStorageView) {
            callbacks.onStorageView(item.action);
          }
          return;
        }
        navigateTo(item.path);
      });
      qaSection.appendChild(div);
    });

    container.appendChild(qaSection);
  }

  // ── Sidebar Item Creator ──────────────────────────────────────────────────

  function createSidebarItem(label, iconSvg, isActive = false) {
    const div = document.createElement('div');
    div.className = 'sidebar-item' + (isActive ? ' active' : '');
    div.innerHTML = `
      <span class="item-icon">${iconSvg}</span>
      <span class="item-label">${Helpers.escapeHtml(label)}</span>
    `;
    return div;
  }

  function createTreeItem(path, label, expanded = false) {
    const wrapper = document.createElement('div');
    wrapper.className = 'tree-node';
    wrapper.dataset.path = path;

    // Item row
    const row = document.createElement('div');
    row.className = 'sidebar-item' + (path === activePath ? ' active' : '');

    // Toggle arrow
    const toggle = document.createElement('span');
    toggle.className = 'tree-toggle' + (expanded ? ' expanded' : '');
    toggle.innerHTML = Icons.chevronRight;
    toggle.addEventListener('click', (e) => {
      e.stopPropagation();
      toggleNode(path);
    });

    // Icon
    const icon = document.createElement('span');
    icon.className = 'item-icon';
    icon.innerHTML = Icons.folder;

    // Label
    const labelEl = document.createElement('span');
    labelEl.className = 'item-label';
    labelEl.textContent = label;

    row.appendChild(toggle);
    row.appendChild(icon);
    row.appendChild(labelEl);

    row.addEventListener('click', (e) => {
      if (!e.target.closest('.tree-toggle')) {
        navigateTo(path);
      }
    });

    wrapper.appendChild(row);

    // Children container
    const children = document.createElement('div');
    children.className = 'tree-children' + (expanded ? ' expanded' : '');
    wrapper.appendChild(children);

    if (expanded) {
      loadTreeChildren(path, children);
    }

    return wrapper;
  }

  // ── Tree Operations ───────────────────────────────────────────────────────

  async function toggleNode(path) {
    const node = container.querySelector(`.tree-node[data-path="${CSS.escape(path)}"]`);
    if (!node) return;

    const toggle = node.querySelector('.tree-toggle');
    const children = node.querySelector('.tree-children');

    if (!toggle || !children) return;

    const isExpanded = toggle.classList.contains('expanded');

    if (isExpanded) {
      toggle.classList.remove('expanded');
      children.classList.remove('expanded');
      treeState[path] = false;
    } else {
      toggle.classList.add('expanded');
      children.classList.add('expanded');
      treeState[path] = true;

      // Load children if empty
      if (children.children.length === 0) {
        await loadTreeChildren(path, children);
      }
    }
  }

  async function loadTreeChildren(path, container) {
    container.innerHTML = '<div class="sidebar-item" style="color:var(--text-tertiary);font-size:var(--text-sm);padding-left:32px;">Loading...</div>';

    try {
      const api = window.App ? window.App.getAPI() : MockAPI;

      // Use path-based ls to avoid changing the API's current directory
      const result = await api.send('ls', { path: path });
      const items = (result.type === 'response' && Array.isArray(result.data)) ? result.data : [];

      container.innerHTML = '';

      const dirs = items.filter(item => item.type === 'dir');

      if (dirs.length === 0) {
        container.innerHTML = '<div class="sidebar-item" style="color:var(--text-tertiary);font-size:var(--text-sm);padding-left:32px;">(empty)</div>';
        return;
      }

      dirs.forEach(dir => {
        const childPath = Helpers.normalizePath(path + '/' + dir.name);
        const childNode = createTreeItem(childPath, dir.name, false);
        container.appendChild(childNode);
      });

      // Cache the directory listing
      directoryCache[path] = items;
    } catch (e) {
      container.innerHTML = '<div class="sidebar-item" style="color:var(--error);font-size:var(--text-sm);padding-left:32px;">Error loading</div>';
      console.error('[Sidebar] Failed to load tree children:', e);
    }
  }

  // ── Navigation ────────────────────────────────────────────────────────────

  function navigateTo(path) {
    setActivePath(path);
    if (callbacks.onNavigate) {
      callbacks.onNavigate(path);
    }
  }

  function setActivePath(path) {
    activePath = path;

    // Update all sidebar items
    container.querySelectorAll('.sidebar-item').forEach(item => {
      const node = item.closest('.tree-node');
      const isActive = node && node.dataset.path === path;

      if (item === node?.querySelector('.sidebar-item')) {
        item.classList.toggle('active', node.dataset.path === path);
      } else if (item.dataset.path) {
        item.classList.toggle('active', item.dataset.path === path);
      } else if (item.dataset.action) {
        item.classList.toggle('active', item.dataset.action === path);
      }
    });
  }

  function getActivePath() {
    return activePath;
  }

  // ── Public API ────────────────────────────────────────────────────────────

  return {
    init,
    navigateTo,
    setActivePath,
    getActivePath,
    toggleNode,
  };
})();

window.Sidebar = Sidebar;
