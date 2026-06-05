/**
 * FlyflyUFS Explorer — Toast Notification System
 * Windows 11 styled toast notifications with stacking.
 * Usage: Toast.success('Message'), Toast.error('Message'), Toast.info('Message'), Toast.warning('Message')
 */
const Toast = (function () {
  'use strict';

  const DURATION = 3000; // auto-dismiss after 3 seconds
  let container = null;
  let toasts = [];

  function getContainer() {
    if (!container) {
      container = document.getElementById('toast-container');
      if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        document.body.appendChild(container);
      }
    }
    return container;
  }

  function show(message, type = 'info', duration = DURATION) {
    const ctr = getContainer();
    const id = Helpers.uid();

    const icons = {
      success: Icons.success,
      error: Icons.error,
      info: Icons.info,
      warning: Icons.warning,
    };

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.dataset.toastId = id;
    toast.innerHTML = `
      <div class="toast-icon">${icons[type] || icons.info}</div>
      <div class="toast-message">${Helpers.escapeHtml(message)}</div>
      <button class="toast-close">${Icons.close}</button>
    `;

    // Close button
    toast.querySelector('.toast-close').addEventListener('click', () => {
      dismiss(id);
    });

    ctr.appendChild(toast);
    toasts.push({ id, toast, timer: null });

    // Auto-dismiss
    if (duration > 0) {
      const timer = setTimeout(() => dismiss(id), duration);
      const entry = toasts.find(t => t.id === id);
      if (entry) entry.timer = timer;
    }

    // Pause on hover
    toast.addEventListener('mouseenter', () => {
      const entry = toasts.find(t => t.id === id);
      if (entry && entry.timer) {
        clearTimeout(entry.timer);
        entry.timer = null;
      }
    });

    toast.addEventListener('mouseleave', () => {
      const entry = toasts.find(t => t.id === id);
      if (entry && !entry.timer) {
        entry.timer = setTimeout(() => dismiss(id), DURATION);
      }
    });

    return id;
  }

  function dismiss(id) {
    const idx = toasts.findIndex(t => t.id === id);
    if (idx < 0) return;

    const { toast, timer } = toasts[idx];
    if (timer) clearTimeout(timer);
    toasts.splice(idx, 1);

    toast.classList.add('toast-dismissing');
    toast.addEventListener('animationend', () => {
      if (toast.parentNode) toast.parentNode.removeChild(toast);
    });
  }

  function success(message, duration) { return show(message, 'success', duration); }
  function error(message, duration)   { return show(message, 'error', duration); }
  function info(message, duration)    { return show(message, 'info', duration); }
  function warning(message, duration) { return show(message, 'warning', duration); }

  return { show, success, error, info, warning, dismiss };
})();

window.Toast = Toast;
