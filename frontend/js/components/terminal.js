/**
 * FlyflyUFS Explorer — Terminal Component
 * A small cmd-like panel that delegates filesystem commands to App.js.
 */
const Terminal = (function () {
  'use strict';

  let container = null;
  let callbacks = {};
  let panel = null;
  let bodyEl = null;
  let outputEl = null;
  let inputEl = null;
  let history = [];
  let historyIndex = -1;
  let lines = [
    { text: 'FlyflyUFS terminal ready. Type `help` for commands.', type: 'muted' },
  ];

  function init(parentEl, cb) {
    container = parentEl;
    callbacks = cb || {};
  }

  function render() {
    container.innerHTML = '';

    panel = document.createElement('div');
    panel.id = 'terminal-panel';
    panel.className = 'terminal-panel';
    panel.innerHTML = `
      <div class="terminal-header">
        <div class="terminal-title">
          <span class="terminal-title-icon">${Icons.terminal}</span>
          <span>FlyflyUFS Terminal</span>
        </div>
      </div>
      <div class="terminal-body">
        <div class="terminal-output" role="log" aria-live="polite"></div>
        <form class="terminal-input-row">
          <span class="terminal-prompt"></span>
          <input class="terminal-input" type="text" autocomplete="off" spellcheck="false" aria-label="Terminal command">
        </form>
      </div>
    `;

    bodyEl = panel.querySelector('.terminal-body');
    outputEl = panel.querySelector('.terminal-output');
    inputEl = panel.querySelector('.terminal-input');

    panel.querySelector('.terminal-input-row').addEventListener('submit', handleSubmit);
    inputEl.addEventListener('keydown', handleHistoryKeys);

    container.appendChild(panel);
    lines.forEach(line => appendLine(line.text, line.type));
    updatePrompt();
  }

  function show() {
    render();
    inputEl.focus();
  }

  function updatePrompt() {
    if (!panel) return;
    const promptEl = panel.querySelector('.terminal-prompt');
    promptEl.textContent = getPromptText();
  }

  async function handleSubmit(e) {
    e.preventDefault();

    const commandLine = inputEl.value.trim();
    if (!commandLine) return;

    print(`${getPromptText()} ${commandLine}`, 'command');
    inputEl.value = '';

    history.push(commandLine);
    historyIndex = history.length;

    if (commandLine === 'clear') {
      lines = [];
      outputEl.innerHTML = '';
      return;
    }

    inputEl.disabled = true;
    try {
      const result = callbacks.onCommand ? await callbacks.onCommand(commandLine) : 'Terminal is not connected.';
      if (result) print(result, 'result');
    } catch (err) {
      print(err && err.message ? err.message : String(err), 'error');
    } finally {
      inputEl.disabled = false;
      updatePrompt();
      inputEl.focus();
      scrollToPrompt();
    }
  }

  function handleHistoryKeys(e) {
    if (e.key === 'ArrowUp') {
      if (history.length === 0) return;
      e.preventDefault();
      historyIndex = Math.max(0, historyIndex - 1);
      inputEl.value = history[historyIndex] || '';
      moveCaretToEnd();
    }

    if (e.key === 'ArrowDown') {
      if (history.length === 0) return;
      e.preventDefault();
      historyIndex = Math.min(history.length, historyIndex + 1);
      inputEl.value = historyIndex === history.length ? '' : history[historyIndex];
      moveCaretToEnd();
    }

  }

  function moveCaretToEnd() {
    requestAnimationFrame(() => {
      inputEl.setSelectionRange(inputEl.value.length, inputEl.value.length);
    });
  }

  function getPromptText() {
    const path = callbacks.getPath ? callbacks.getPath() : '/';
    const username = callbacks.getUsername ? callbacks.getUsername() : 'root';
    return `${username || 'root'}@FlyflyUFS ${path}>`;
  }

  function print(text, type) {
    lines.push({ text, type });
    appendLine(text, type);
  }

  function appendLine(text, type) {
    if (!outputEl) return;
    const line = document.createElement('div');
    line.className = 'terminal-line' + (type ? ` ${type}` : '');
    line.textContent = text;
    outputEl.appendChild(line);
    scrollToPrompt();
  }

  function scrollToPrompt() {
    if (!bodyEl) return;
    bodyEl.scrollTop = bodyEl.scrollHeight;
  }

  return {
    init,
    show,
    updatePrompt,
  };
})();

window.Terminal = Terminal;
