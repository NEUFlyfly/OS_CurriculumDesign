/**
 * FlyflyUFS Explorer — Utility Helpers
 * Formatting, sorting, debounce, and DOM helpers.
 */

const Helpers = {
  /**
   * Format bytes to human-readable string.
   */
  formatSize(bytes) {
    if (bytes == null || bytes === 0) return '0 B';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    let i = 0;
    let size = bytes;
    while (size >= 1024 && i < units.length - 1) {
      size /= 1024;
      i++;
    }
    return i === 0 ? `${size} ${units[i]}` : `${size.toFixed(1)} ${units[i]}`;
  },

  /**
   * Format a date string or timestamp to locale date string.
   */
  formatDate(date) {
    if (!date) return '';
    const d = date instanceof Date ? date : new Date(date);
    if (isNaN(d.getTime())) return String(date);
    return d.toLocaleDateString('en-US', { year: 'numeric', month: 'short', day: 'numeric' })
      + ' ' + d.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
  },

  /**
   * Format permissions string to rwx representation.
   */
  formatPermissions(permStr) {
    if (!permStr) return '---------';
    const octal = parseInt(permStr, 8);
    if (isNaN(octal)) return permStr;
    const rwx = ['---', '--x', '-w-', '-wx', 'r--', 'r-x', 'rw-', 'rwx'];
    const owner = (octal >> 6) & 7;
    const group = (octal >> 3) & 7;
    const other = octal & 7;
    return rwx[owner] + rwx[group] + rwx[other];
  },

  /**
   * Debounce a function call.
   */
  debounce(fn, delay = 200) {
    let timer;
    return function (...args) {
      clearTimeout(timer);
      timer = setTimeout(() => fn.apply(this, args), delay);
    };
  },

  /**
   * Throttle a function call.
   */
  throttle(fn, limit = 100) {
    let inThrottle = false;
    return function (...args) {
      if (!inThrottle) {
        fn.apply(this, args);
        inThrottle = true;
        setTimeout(() => { inThrottle = false; }, limit);
      }
    };
  },

  /**
   * Escape HTML to prevent XSS.
   */
  escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  },

  /**
   * Generate a simple unique ID.
   */
  uid() {
    return Date.now().toString(36) + Math.random().toString(36).substr(2, 6);
  },

  /**
   * Normalize a path: ensure it starts with / and remove trailing slash.
   */
  normalizePath(path) {
    if (!path) return '/';
    path = '/' + path.replace(/^\/+|\/+$/g, '');
    return path === '' ? '/' : path;
  },

  /**
   * Get parent path.
   */
  parentPath(path) {
    const normalized = Helpers.normalizePath(path);
    if (normalized === '/') return '/';
    const parts = normalized.split('/').filter(Boolean);
    parts.pop();
    return '/' + parts.join('/');
  },

  /**
   * Get the last segment of a path.
   */
  basename(path) {
    if (!path || path === '/') return '/';
    const parts = path.replace(/\/+$/, '').split('/');
    return parts[parts.length - 1] || '/';
  },

  /**
   * Sort files: directories first, then alphabetically.
   */
  sortFiles(items, { sortBy = 'name', sortDir = 'asc' } = {}) {
    return [...items].sort((a, b) => {
      // Directories always first
      if (a.type === 'dir' && b.type !== 'dir') return -1;
      if (a.type !== 'dir' && b.type === 'dir') return 1;

      let cmp = 0;
      switch (sortBy) {
        case 'name':
          cmp = a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' });
          break;
        case 'date':
          cmp = (a.mtime || 0) - (b.mtime || 0);
          break;
        case 'type':
          cmp = (a.type || '').localeCompare(b.type || '');
          if (cmp === 0) cmp = a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' });
          break;
        case 'size':
          cmp = (a.size || 0) - (b.size || 0);
          break;
        default:
          cmp = a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' });
      }
      return sortDir === 'desc' ? -cmp : cmp;
    });
  },

  /**
   * Simple event emitter mixin.
   */
  createEventEmitter() {
    const listeners = {};
    return {
      on(event, fn) {
        (listeners[event] = listeners[event] || []).push(fn);
        return () => this.off(event, fn);
      },
      off(event, fn) {
        const list = listeners[event];
        if (list) {
          const idx = list.indexOf(fn);
          if (idx >= 0) list.splice(idx, 1);
        }
      },
      emit(event, ...args) {
        (listeners[event] || []).forEach(fn => fn(...args));
      }
    };
  },
};

window.Helpers = Helpers;
