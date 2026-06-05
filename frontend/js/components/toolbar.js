/**
 * FlyflyUFS Explorer — Toolbar Component
 * Address bar, search, navigation and action buttons.
 * Emits events that App.js listens to.
 */
const Toolbar = (function () {
  'use strict';

  let container = null;
  let callbacks = {};
  let viewMode = 'grid'; // 'grid' | 'list'
  let canGoBack = false;
  let canGoForward = false;

  function init(parentEl, cb) {
    callbacks = cb || {};
    container = parentEl;

    render();
    bindEvents();
  }

  function render() {
    container.innerHTML = `
      <!-- Navigation -->
      <button class="toolbar-btn" id="btn-back" title="Back (Alt+Left)" disabled>${Icons.back}</button>
      <button class="toolbar-btn" id="btn-forward" title="Forward (Alt+Right)" disabled>${Icons.forward}</button>
      <button class="toolbar-btn" id="btn-up" title="Up to parent directory">${Icons.up}</button>
      <button class="toolbar-btn" id="btn-refresh" title="Refresh (F5)">${Icons.refresh}</button>

      <div class="toolbar-divider"></div>

      <!-- Address Bar -->
      <div id="address-bar-container">
        <span class="path-icon">${Icons.path}</span>
        <input type="text" id="address-bar" placeholder="Enter a path..." spellcheck="false" />
      </div>

      <!-- Search -->
      <div id="search-container">
        <span class="search-icon">${Icons.search}</span>
        <input type="text" id="search-input" placeholder="Search" spellcheck="false" />
      </div>

      <div class="toolbar-divider"></div>

      <!-- Actions -->
      <button class="toolbar-btn" id="btn-new-folder" title="New Folder">${Icons.newFolder}</button>
      <button class="toolbar-btn" id="btn-new-file" title="New File">${Icons.newFile}</button>
      <button class="toolbar-btn" id="btn-rename" title="Rename (F2)">${Icons.rename}</button>
      <button class="toolbar-btn" id="btn-delete" title="Delete (Del)">${Icons.delete}</button>

      <button class="toolbar-btn" id="btn-properties" title="Properties">${Icons.properties}</button>

      <div class="toolbar-divider"></div>

      <!-- View Toggle -->
      <div class="view-toggle-group">
        <button class="view-toggle-btn active" id="btn-view-grid" title="Grid View">${Icons.gridView}</button>
        <button class="view-toggle-btn" id="btn-view-list" title="List View">${Icons.listView}</button>
      </div>
    `;
  }

  function bindEvents() {
    // Navigation
    container.querySelector('#btn-back').addEventListener('click', () => {
      if (callbacks.onNavigateBack) callbacks.onNavigateBack();
    });
    container.querySelector('#btn-forward').addEventListener('click', () => {
      if (callbacks.onNavigateForward) callbacks.onNavigateForward();
    });
    container.querySelector('#btn-up').addEventListener('click', () => {
      if (callbacks.onNavigateUp) callbacks.onNavigateUp();
    });
    container.querySelector('#btn-refresh').addEventListener('click', () => {
      if (callbacks.onRefresh) callbacks.onRefresh();
    });

    // Address bar
    const addressBar = container.querySelector('#address-bar');
    addressBar.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        addressBar.blur();
        if (callbacks.onNavigateTo) callbacks.onNavigateTo(addressBar.value.trim());
      }
      if (e.key === 'Escape') {
        addressBar.blur();
        if (callbacks.onAddressBarUpdate) callbacks.onAddressBarUpdate();
      }
    });
    addressBar.addEventListener('focus', () => {
      addressBar.select();
    });

    // Search
    const searchInput = container.querySelector('#search-input');
    searchInput.addEventListener('input', Helpers.debounce((e) => {
      if (callbacks.onSearch) callbacks.onSearch(e.target.value.trim());
    }, 200));
    searchInput.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        searchInput.value = '';
        searchInput.blur();
        if (callbacks.onSearch) callbacks.onSearch('');
      }
    });

    // Action buttons
    container.querySelector('#btn-new-folder').addEventListener('click', () => {
      if (callbacks.onNewFolder) callbacks.onNewFolder();
    });
    container.querySelector('#btn-new-file').addEventListener('click', () => {
      if (callbacks.onNewFile) callbacks.onNewFile();
    });
    container.querySelector('#btn-rename').addEventListener('click', () => {
      if (callbacks.onRename) callbacks.onRename();
    });
    container.querySelector('#btn-delete').addEventListener('click', () => {
      if (callbacks.onDelete) callbacks.onDelete();
    });
    container.querySelector('#btn-properties').addEventListener('click', () => {
      if (callbacks.onProperties) callbacks.onProperties();
    });

    // View toggle
    container.querySelector('#btn-view-grid').addEventListener('click', () => {
      setViewMode('grid');
    });
    container.querySelector('#btn-view-list').addEventListener('click', () => {
      setViewMode('list');
    });
  }

  function setViewMode(mode) {
    viewMode = mode;
    const btnGrid = container.querySelector('#btn-view-grid');
    const btnList = container.querySelector('#btn-view-list');

    btnGrid.classList.toggle('active', mode === 'grid');
    btnList.classList.toggle('active', mode === 'list');

    if (callbacks.onViewModeChange) callbacks.onViewModeChange(mode);
  }

  function setAddress(path) {
    const addressBar = container.querySelector('#address-bar');
    if (addressBar && document.activeElement !== addressBar) {
      addressBar.value = path;
    }
  }

  function getAddress() {
    const addressBar = container.querySelector('#address-bar');
    return addressBar ? addressBar.value : '/';
  }

  function setNavigationState(back, forward) {
    canGoBack = back;
    canGoForward = forward;
    const btnBack = container.querySelector('#btn-back');
    const btnForward = container.querySelector('#btn-forward');
    if (btnBack) btnBack.disabled = !back;
    if (btnForward) btnForward.disabled = !forward;
  }

  function getViewMode() {
    return viewMode;
  }

  function setSearch(value) {
    const searchInput = container.querySelector('#search-input');
    if (searchInput && document.activeElement !== searchInput) {
      searchInput.value = value || '';
    }
  }

  function focusAddressBar() {
    const addressBar = container.querySelector('#address-bar');
    if (addressBar) {
      addressBar.focus();
      addressBar.select();
    }
  }

  return {
    init,
    setAddress,
    getAddress,
    setViewMode,
    getViewMode,
    setNavigationState,
    setSearch,
    focusAddressBar,
  };
})();

window.Toolbar = Toolbar;
