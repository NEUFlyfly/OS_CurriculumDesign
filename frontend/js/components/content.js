/**
 * FlyflyUFS Explorer — Content Area Component
 * Grid and list views for displaying files and folders.
 * Handles selection, inline rename/create, sorting, and context menu triggers.
 */
const ContentArea = (function () {
  'use strict';

  let container = null;
  let callbacks = {};
  let items = [];
  let selectedItems = new Set(); // set of item names
  let viewMode = 'grid';
  let sortBy = 'name';
  let sortDir = 'asc';
  let searchQuery = '';
  let inlineInput = null;

  // ── Initialization ────────────────────────────────────────────────────────

  function init(parentEl, cb) {
    container = parentEl;
    callbacks = cb || {};
    container.addEventListener('contextmenu', handleDelegatedContextMenu, true);
    renderEmpty();
  }

  // ── Render ────────────────────────────────────────────────────────────────

  function setItems(newItems, query = '') {
    items = newItems || [];
    searchQuery = query || '';
    render();
  }

  function render() {
    if (!container) return;

    // Filter by search
    let filtered = items;
    if (searchQuery) {
      const q = searchQuery.toLowerCase();
      filtered = items.filter(item => item.name.toLowerCase().includes(q));
    }

    // Sort
    filtered = Helpers.sortFiles(filtered, { sortBy, sortDir });

    // Clear selection for items no longer present
    const currentNames = new Set(filtered.map(i => i.name));
    selectedItems.forEach(name => {
      if (!currentNames.has(name)) selectedItems.delete(name);
    });

    if (filtered.length === 0) {
      if (searchQuery) {
        renderNoResults();
      } else {
        renderEmpty();
      }
      return;
    }

    if (viewMode === 'grid') {
      renderGridView(filtered);
    } else {
      renderListView(filtered);
    }
  }

  function renderEmpty() {
    container.innerHTML = `
      <div class="empty-state">
        ${Icons.emptyFolder}
        <div class="empty-text">This folder is empty</div>
        <div class="empty-subtext">Create a new file or folder to get started</div>
      </div>
    `;
  }

  function renderNoResults() {
    container.innerHTML = `
      <div class="empty-state">
        ${Icons.search}
        <div class="empty-text">No results found</div>
        <div class="empty-subtext">No items match "${Helpers.escapeHtml(searchQuery)}"</div>
      </div>
    `;
  }

  // ── Grid View ─────────────────────────────────────────────────────────────

  function renderGridView(filtered) {
    // Close any inline input
    closeInlineInput();

    let html = '<div class="content-grid">';

    filtered.forEach(item => {
      const isSelected = selectedItems.has(item.name);
      const icon = item.type === 'dir' ? Icons.folder : getFileIcon(item.name);
      html += `
        <div class="file-item${isSelected ? ' selected' : ''}" data-name="${Helpers.escapeHtml(item.name)}" data-type="${item.type}">
          <div class="file-icon">${icon}</div>
          <div class="file-name" title="${Helpers.escapeHtml(item.name)}">${Helpers.escapeHtml(item.name)}</div>
        </div>
      `;
    });

    html += '</div>';
    container.innerHTML = html;

    bindItemEvents();
  }

  // ── List View ─────────────────────────────────────────────────────────────

  function renderListView(filtered) {
    closeInlineInput();

    const columns = [
      { key: 'name', label: 'Name', flex: 3 },
      { key: 'date', label: 'Date Modified', flex: 1 },
      { key: 'type', label: 'Type', flex: 1 },
      { key: 'size', label: 'Size', flex: 1 },
    ];

    let html = '<div class="content-list">';

    // Header
    html += '<div class="content-list-header">';
    columns.forEach(col => {
      const cls = col.key === sortBy ? (sortDir === 'asc' ? 'sort-asc' : 'sort-desc') : '';
      html += `<div class="col col-${col.key} ${cls}" data-sort="${col.key}" style="flex:${col.flex}">${col.label}</div>`;
    });
    html += '</div>';

    // Rows
    filtered.forEach(item => {
      const isSelected = selectedItems.has(item.name);
      const icon = item.type === 'dir' ? Icons.folder : getFileIcon(item.name);
      html += `
        <div class="list-row${isSelected ? ' selected' : ''}" data-name="${Helpers.escapeHtml(item.name)}" data-type="${item.type}">
          <div class="col-name" style="flex:3">
            <span class="file-icon">${icon}</span>
            <span>${Helpers.escapeHtml(item.name)}</span>
          </div>
          <div class="col-date" style="flex:1">${Helpers.formatDate(item.mtime)}</div>
          <div class="col-type" style="flex:1">${item.type === 'dir' ? 'File folder' : (item.name.split('.').pop().toUpperCase() + ' File')}</div>
          <div class="col-size" style="flex:1">${item.type === 'dir' ? '' : Helpers.formatSize(item.size)}</div>
        </div>
      `;
    });

    html += '</div>';
    container.innerHTML = html;

    // Header click events for sorting
    container.querySelectorAll('.content-list-header .col').forEach(col => {
      col.addEventListener('click', () => {
        const key = col.dataset.sort;
        if (sortBy === key) {
          sortDir = sortDir === 'asc' ? 'desc' : 'asc';
        } else {
          sortBy = key;
          sortDir = 'asc';
        }
        render();
      });
    });

    bindItemEvents();
  }

  // ── Item Event Binding ────────────────────────────────────────────────────

  function bindItemEvents() {
    // Grid items
    container.querySelectorAll('.file-item').forEach(el => {
      el.addEventListener('click', (e) => handleItemClick(e, el));
      el.addEventListener('dblclick', (e) => handleItemDblClick(e, el));
    });

    // List rows
    container.querySelectorAll('.list-row').forEach(el => {
      el.addEventListener('click', (e) => handleItemClick(e, el));
      el.addEventListener('dblclick', (e) => handleItemDblClick(e, el));
    });

    // Deselect on click outside
    container.addEventListener('click', (e) => {
      if (e.target === container || e.target.classList.contains('content-grid') || e.target.classList.contains('content-list')) {
        clearSelection();
      }
    });
  }

  function handleDelegatedContextMenu(e) {
    const itemEl = e.target.closest('.file-item, .list-row');
    if (itemEl && container.contains(itemEl)) {
      e.stopPropagation();
      handleContextMenu(e, itemEl);
      return;
    }

    if (e.target === container || container.contains(e.target)) {
      e.stopPropagation();
      handleEmptySpaceContextMenu(e);
    }
  }

  function handleItemClick(e, el) {
    const name = el.dataset.name;

    if (e.ctrlKey || e.metaKey) {
      // Toggle selection
      if (selectedItems.has(name)) {
        selectedItems.delete(name);
      } else {
        selectedItems.add(name);
      }
    } else if (e.shiftKey && selectedItems.size > 0) {
      // Shift-range selection (simplified: select this item + last selected)
      selectedItems.add(name);
    } else {
      // Single selection
      clearSelection();
      selectedItems.add(name);
    }

    updateSelectionUI();
    if (callbacks.onSelectionChange) {
      callbacks.onSelectionChange(Array.from(selectedItems));
    }
  }

  function handleItemDblClick(e, el) {
    const name = el.dataset.name;
    const type = el.dataset.type;

    if (type === 'dir') {
      if (callbacks.onOpenFolder) callbacks.onOpenFolder(name);
    } else {
      if (callbacks.onOpenFile) callbacks.onOpenFile(name);
    }
  }

  function handleContextMenu(e, el) {
    e.preventDefault();
    const name = el.dataset.name;
    const type = el.dataset.type;

    // Select item if not selected
    if (!selectedItems.has(name)) {
      clearSelection();
      selectedItems.add(name);
      updateSelectionUI();
    }

    if (callbacks.onContextMenu) {
      callbacks.onContextMenu(e.clientX, e.clientY, name, type);
    }
  }

  function handleEmptySpaceContextMenu(e) {
    e.preventDefault();
    if (callbacks.onContextMenu) {
      callbacks.onContextMenu(e.clientX, e.clientY, null, null);
    }
  }

  // ── Selection ─────────────────────────────────────────────────────────────

  function updateSelectionUI() {
    container.querySelectorAll('.file-item, .list-row').forEach(el => {
      const name = el.dataset.name;
      el.classList.toggle('selected', selectedItems.has(name));
    });
  }

  function clearSelection() {
    selectedItems.clear();
    updateSelectionUI();
    if (callbacks.onSelectionChange) {
      callbacks.onSelectionChange([]);
    }
  }

  function getSelectedItems() {
    return items.filter(item => selectedItems.has(item.name));
  }

  function getSelectedNames() {
    return Array.from(selectedItems);
  }

  function selectAll() {
    const names = searchQuery
      ? items.filter(i => i.name.toLowerCase().includes(searchQuery.toLowerCase())).map(i => i.name)
      : items.map(i => i.name);
    selectedItems = new Set(names);
    updateSelectionUI();
    if (callbacks.onSelectionChange) {
      callbacks.onSelectionChange(Array.from(selectedItems));
    }
  }

  function selectOnly(name) {
    clearSelection();
    if (name) selectedItems.add(name);
    updateSelectionUI();
    if (callbacks.onSelectionChange) {
      callbacks.onSelectionChange(Array.from(selectedItems));
    }
  }

  // ── Inline Input (Rename / Create) ────────────────────────────────────────

  function startRename(name) {
    closeInlineInput();

    // Find the element
    const el = container.querySelector(`[data-name="${CSS.escape(name)}"]`);
    if (!el) return;

    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'inline-input';
    input.value = name;
    el.style.position = 'relative';
    el.appendChild(input);
    input.focus();
    input.select();

    // If it's a directory rename, select name without extension context
    const dotIndex = name.lastIndexOf('.');
    if (dotIndex > 0) {
      input.setSelectionRange(0, dotIndex);
    }

    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        const newName = input.value.trim();
        closeInlineInput();
        if (newName && newName !== name) {
          if (callbacks.onRenameItem) callbacks.onRenameItem(name, newName);
        }
      } else if (e.key === 'Escape') {
        e.preventDefault();
        closeInlineInput();
      }
    });

    input.addEventListener('blur', () => {
      // Slight delay to allow Enter key to fire first
      setTimeout(() => {
        const newName = input.value.trim();
        closeInlineInput();
        if (newName && newName !== name) {
          if (callbacks.onRenameItem) callbacks.onRenameItem(name, newName);
        }
      }, 100);
    });

    inlineInput = input;
  }

  function startCreate(type = 'folder') {
    closeInlineInput();

    const defaultName = type === 'folder' ? 'New Folder' : 'New File.txt';
    const placeholder = type === 'folder' ? 'Folder name...' : 'File name...';

    // Find the content container
    const grid = container.querySelector('.content-grid');
    const list = container.querySelector('.content-list');

    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'inline-input';
    input.placeholder = placeholder;
    input.value = defaultName;

    if (grid) {
      // Add to grid
      const placeholderEl = document.createElement('div');
      placeholderEl.className = 'file-item creating';
      placeholderEl.innerHTML = '<div class="file-icon">' + (type === 'folder' ? Icons.folder : Icons.fileDefault) + '</div>';
      placeholderEl.style.position = 'relative';
      placeholderEl.appendChild(input);
      grid.insertBefore(placeholderEl, grid.firstChild);
    } else if (list) {
      const placeholderEl = document.createElement('div');
      placeholderEl.className = 'list-row creating';
      placeholderEl.style.display = 'flex';
      placeholderEl.style.alignItems = 'center';
      placeholderEl.style.padding = '5px 12px';
      placeholderEl.innerHTML = '<span class="file-icon" style="margin-right:8px">' + (type === 'folder' ? Icons.folder : Icons.fileDefault) + '</span>';
      placeholderEl.appendChild(input);
      input.style.cssText = 'flex:1;border:none;outline:none;font-family:var(--font-family);font-size:var(--text-base);background:transparent;';
      list.insertBefore(placeholderEl, list.children[1] || list.firstChild);
    } else {
      return; // empty state - use callback instead
    }

    input.focus();
    input.select();

    const cleanup = () => {
      const parent = input.parentElement;
      if (parent) parent.remove();
      inlineInput = null;
    };

    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        const name = input.value.trim();
        if (name) {
          cleanup();
          if (type === 'folder') {
            if (callbacks.onCreateFolder) callbacks.onCreateFolder(name);
          } else {
            if (callbacks.onCreateFile) callbacks.onCreateFile(name);
          }
        } else {
          cleanup();
        }
      } else if (e.key === 'Escape') {
        e.preventDefault();
        cleanup();
      }
    });

    input.addEventListener('blur', () => {
      setTimeout(() => {
        if (inlineInput === input) {
          const name = input.value.trim();
          cleanup();
          if (name) {
            if (type === 'folder') {
              if (callbacks.onCreateFolder) callbacks.onCreateFolder(name);
            } else {
              if (callbacks.onCreateFile) callbacks.onCreateFile(name);
            }
          }
        }
      }, 100);
    });

    inlineInput = input;
  }

  function closeInlineInput() {
    if (inlineInput) {
      const parent = inlineInput.parentElement;
      if (parent && parent.classList.contains('creating')) {
        parent.remove();
      } else {
        inlineInput.remove();
      }
      inlineInput = null;
    }
  }

  function isInlineInputActive() {
    return inlineInput !== null;
  }

  // ── View Mode ─────────────────────────────────────────────────────────────

  function setViewMode(mode) {
    viewMode = mode;
    if (items.length > 0) render();
  }

  // ── Sort ──────────────────────────────────────────────────────────────────

  function setSort(by, dir) {
    sortBy = by;
    sortDir = dir || 'asc';
    if (items.length > 0) render();
  }

  // ── Public API ────────────────────────────────────────────────────────────

  return {
    init,
    setItems,
    clearSelection,
    getSelectedItems,
    getSelectedNames,
    selectAll,
    selectOnly,
    setViewMode,
    setSort,
    startRename,
    startCreate,
    closeInlineInput,
    isInlineInputActive,
    get items() { return items; },
  };
})();

window.ContentArea = ContentArea;
