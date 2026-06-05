/**
 * FlyflyUFS Explorer — Status Bar Component
 * Displays item count, selection count, storage info, and view mode.
 */
const StatusBar = (function () {
  'use strict';

  let container = null;
  let diskInfo = null;

  function init(parentEl) {
    container = parentEl;
    render();
    return { update };
  }

  function render() {
    container.innerHTML = `
      <span id="status-item-count">0 items</span>
      <span id="status-selection" class="hidden">| <span id="status-selection-count"></span></span>
      <div class="disk-usage-bar">
        <span class="usage-text" id="status-disk-text">Storage: --</span>
        <div class="usage-progress">
          <div class="usage-fill" id="status-disk-fill" style="width:0%"></div>
        </div>
      </div>
    `;
  }

  function update(state) {
    if (!container) return;
    state = state || {};

    // Item count
    const itemCount = container.querySelector('#status-item-count');
    const total = (state.folderCount || 0) + (state.fileCount || 0);
    if (itemCount) {
      itemCount.textContent = `${total} item${total !== 1 ? 's' : ''}`;
    }

    // Selection count
    const selectionEl = container.querySelector('#status-selection');
    const selectionCount = container.querySelector('#status-selection-count');
    if (selectionEl && selectionCount) {
      const selCount = state.selectedCount || 0;
      if (selCount > 0) {
        selectionEl.classList.remove('hidden');
        selectionCount.textContent = `${selCount} item${selCount !== 1 ? 's' : ''} selected`;
      } else {
        selectionEl.classList.add('hidden');
      }
    }
  }

  function setDiskUsage(info) {
    diskInfo = info;
    if (!container || !info) return;

    const diskText = container.querySelector('#status-disk-text');
    const diskFill = container.querySelector('#status-disk-fill');

    if (diskText && info.totalBlocks && info.freeBlocks != null) {
      const totalBytes = info.totalBlocks * (info.blockSize || 512);
      const freeBytes = info.freeBlocks * (info.blockSize || 512);
      const usedBytes = totalBytes - freeBytes;
      const pct = totalBytes > 0 ? (usedBytes / totalBytes) * 100 : 0;

      diskText.textContent = `Storage: ${Helpers.formatSize(freeBytes)} free of ${Helpers.formatSize(totalBytes)}`;
      if (diskFill) {
        diskFill.style.width = Math.min(pct, 100) + '%';
      }
    }
  }

  return { init, update, setDiskUsage };
})();

window.StatusBar = StatusBar;
