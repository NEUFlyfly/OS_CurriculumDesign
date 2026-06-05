/**
 * FlyflyUFS Explorer — Context Menu
 * Windows 11 styled right-click context menu with keyboard navigation.
 * Usage: ContextMenu.show(x, y, items)
 *   items = [{ label, icon?, shortcut?, action, separator? }]
 */
const ContextMenu = (function () {
  'use strict';

  let menuEl = null;
  let isVisible = false;
  let currentItems = [];
  let focusedIndex = -1;
  let resolvePromise = null;

  function createMenuElement() {
    if (menuEl) return menuEl;
    menuEl = document.createElement('div');
    menuEl.id = 'context-menu';
    menuEl.style.display = 'none';
    document.body.appendChild(menuEl);
    return menuEl;
  }

  function buildMenu(items) {
    const el = createMenuElement();
    el.innerHTML = '';
    focusedIndex = -1;
    currentItems = [];

    items.forEach((item, index) => {
      if (item.separator) {
        const sep = document.createElement('div');
        sep.className = 'context-menu-separator';
        el.appendChild(sep);
        currentItems.push({ separator: true });
        return;
      }

      const div = document.createElement('div');
      div.className = 'context-menu-item';
      div.dataset.index = index;

      if (item.icon) {
        div.innerHTML += `<span class="menu-icon">${item.icon}</span>`;
      }
      div.innerHTML += `<span class="menu-label">${Helpers.escapeHtml(item.label)}</span>`;
      if (item.shortcut) {
        div.innerHTML += `<span class="menu-shortcut">${item.shortcut}</span>`;
      }

      div.addEventListener('click', (e) => {
        e.stopPropagation();
        if (item.action) item.action();
        hide();
      });

      div.addEventListener('mouseenter', () => {
        focusItem(index);
      });

      el.appendChild(div);
      currentItems.push({ ...item, element: div });
    });
  }

  function focusItem(index) {
    // Remove previous focus
    currentItems.forEach((item, i) => {
      if (item.element && i === focusedIndex) {
        item.element.classList.remove('focused');
      }
    });

    // Skip separators
    while (index >= 0 && index < currentItems.length && currentItems[index].separator) {
      index = index < focusedIndex ? index - 1 : index + 1;
    }

    if (index < 0 || index >= currentItems.length) return;

    focusedIndex = index;
    const item = currentItems[index];
    if (item.element) {
      item.element.classList.add('focused');
      item.element.scrollIntoView({ block: 'nearest' });
    }
  }

  function show(x, y, items) {
    if (isVisible) hide();

    buildMenu(items);

    const el = menuEl;
    el.style.display = 'block';

    // Position: ensure menu fits in viewport
    const menuWidth = el.offsetWidth || 210;
    const menuHeight = el.offsetHeight || 200;

    let posX = x;
    let posY = y;

    if (posX + menuWidth > window.innerWidth) {
      posX = window.innerWidth - menuWidth - 4;
    }
    if (posY + menuHeight > window.innerHeight) {
      posY = window.innerHeight - menuHeight - 4;
    }
    if (posX < 0) posX = 4;
    if (posY < 0) posY = 4;

    el.style.left = posX + 'px';
    el.style.top = posY + 'px';

    // Re-trigger animation
    el.style.animation = 'none';
    el.offsetHeight; // force reflow
    el.style.animation = '';

    isVisible = true;
    focusedIndex = -1;
    if (currentItems.length > 0) focusItem(0);

    // Return a promise that resolves with the selected action
    return new Promise((resolve) => {
      resolvePromise = resolve;
    });
  }

  function hide() {
    if (!isVisible) return;
    isVisible = false;
    if (menuEl) {
      menuEl.style.display = 'none';
    }
    currentItems = [];
    focusedIndex = -1;
    if (resolvePromise) {
      resolvePromise(null);
      resolvePromise = null;
    }
  }

  function handleKeyDown(e) {
    if (!isVisible) return;

    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        focusItem(focusedIndex + 1);
        break;
      case 'ArrowUp':
        e.preventDefault();
        focusItem(focusedIndex - 1);
        break;
      case 'Enter':
        e.preventDefault();
        if (focusedIndex >= 0 && focusedIndex < currentItems.length) {
          const item = currentItems[focusedIndex];
          if (item.action) item.action();
          hide();
        }
        break;
      case 'Escape':
        e.preventDefault();
        hide();
        break;
    }
  }

  // Global click handler to close menu
  document.addEventListener('click', (e) => {
    if (isVisible && menuEl && !menuEl.contains(e.target)) {
      hide();
    }
  });

  // Global context menu prevention (allow our custom menu)
  document.addEventListener('contextmenu', (e) => {
    if (isVisible && menuEl && !menuEl.contains(e.target)) {
      hide();
    }
  });

  // Keyboard handler
  document.addEventListener('keydown', handleKeyDown);

  return { show, hide, get isVisible() { return isVisible; } };
})();

window.ContextMenu = ContextMenu;
