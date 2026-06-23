/**
 * FlyflyUFS Explorer - Performance Monitoring View
 * Shows backend command timing and client WebSocket round-trip timing.
 */
const PerformanceView = (function () {
  'use strict';

  let container = null;
  let callbacks = {};

  function init(parentEl, cb) {
    container = parentEl;
    callbacks = cb || {};
  }

  async function show() {
    renderLoading();

    try {
      const stats = await callbacks.fetchStats();
      render(stats || {});
    } catch (e) {
      renderError(e);
    }
  }

  function refresh() {
    return show();
  }

  function renderLoading() {
    if (!container) return;
    container.innerHTML = '<div class="performance-view loading-state">正在读取性能数据...</div>';
  }

  function renderError(error) {
    if (!container) return;
    const message = error && error.message ? error.message : '无法读取性能数据';
    container.innerHTML = `
      <div class="performance-view performance-error">
        <div class="performance-title">读取失败</div>
        <div class="performance-description">${Helpers.escapeHtml(message)}</div>
      </div>
    `;
  }

  function render(stats) {
    if (!container) return;
    const backend = stats.backend || {};
    const client = stats.client || {};
    const backendAggregates = normalizeAggregates(backend.aggregates);
    const clientAggregates = normalizeAggregates(client.aggregates);
    const recent = mergeRecentRecords(backend.recent, client.recent);

    container.innerHTML = `
      <div class="performance-view">
        <div class="performance-header">
          <div>
            <div class="performance-title">性能监控</div>
            <div class="performance-description">记录当前会话的后端命令耗时和 WebSocket 往返时间。</div>
          </div>
          <button class="performance-refresh" id="performance-refresh">刷新</button>
        </div>
        ${renderSummary(backendAggregates, clientAggregates, backend.slowest, client.slowest)}
        <div class="performance-content">
          ${renderAggregateTable(backendAggregates, clientAggregates)}
          ${renderRecentTable(recent)}
        </div>
      </div>
    `;

    const refreshButton = container.querySelector('#performance-refresh');
    if (refreshButton) refreshButton.addEventListener('click', refresh);
  }

  function renderSummary(backendAggregates, clientAggregates, backendSlowest, clientSlowest) {
    const backendTotal = totalCount(backendAggregates);
    const backendAvg = averageTotal(backendAggregates);
    const clientAvg = averageTotal(clientAggregates);
    const slowest = backendSlowest && backendSlowest.command
      ? `${backendSlowest.command} · ${formatMs(backendSlowest.backendMs)}`
      : '--';
    const clientSlowestText = clientSlowest && clientSlowest.command
      ? `${clientSlowest.command} · ${formatMs(clientSlowest.roundTripMs)}`
      : '--';

    return `
      <div class="performance-summary">
        <div class="performance-card">
          <span>后端命令次数</span>
          <strong>${backendTotal}</strong>
        </div>
        <div class="performance-card">
          <span>后端平均耗时</span>
          <strong>${formatMs(backendAvg)}</strong>
        </div>
        <div class="performance-card">
          <span>后端最慢命令</span>
          <strong>${Helpers.escapeHtml(slowest)}</strong>
        </div>
        <div class="performance-card">
          <span>客户端平均往返</span>
          <strong>${formatMs(clientAvg)}</strong>
          <em>${Helpers.escapeHtml(clientSlowestText)}</em>
        </div>
      </div>
    `;
  }

  function renderAggregateTable(backendAggregates, clientAggregates) {
    const commands = Array.from(new Set([
      ...Object.keys(backendAggregates),
      ...Object.keys(clientAggregates),
    ])).sort();

    if (commands.length === 0) {
      return '<div class="performance-empty">暂无性能记录。执行几个命令后再刷新本页。</div>';
    }

    const rows = commands.map(command => {
      const backend = backendAggregates[command];
      const client = clientAggregates[command];
      return `
        <tr>
          <td>${Helpers.escapeHtml(command)}</td>
          <td>${backend ? backend.count : '--'}</td>
          <td>${backend ? formatMs(backend.avgMs) : '--'}</td>
          <td>${backend ? formatMs(backend.minMs) : '--'}</td>
          <td>${backend ? formatMs(backend.maxMs) : '--'}</td>
          <td>${client ? formatMs(client.avgMs) : '--'}</td>
          <td>${client ? formatMs(client.maxMs) : '--'}</td>
        </tr>
      `;
    }).join('');

    return `
      <section class="performance-section">
        <h3>命令聚合统计</h3>
        <div class="performance-table-wrap">
          <table class="performance-table">
            <thead>
              <tr>
                <th>命令</th>
                <th>次数</th>
                <th>后端平均</th>
                <th>后端最小</th>
                <th>后端最大</th>
                <th>往返平均</th>
                <th>往返最大</th>
              </tr>
            </thead>
            <tbody>${rows}</tbody>
          </table>
        </div>
      </section>
    `;
  }

  function renderRecentTable(records) {
    if (records.length === 0) return '';

    const rows = records.slice(-50).reverse().map(record => `
      <tr>
        <td>${Helpers.escapeHtml(record.command || '--')}</td>
        <td><span class="performance-status ${record.status === 'error' ? 'is-error' : 'is-ok'}">${Helpers.escapeHtml(record.status || '--')}</span></td>
        <td>${record.backendMs == null ? '--' : formatMs(record.backendMs)}</td>
        <td>${record.roundTripMs == null ? '--' : formatMs(record.roundTripMs)}</td>
        <td>${Helpers.escapeHtml(record.timestamp || '--')}</td>
      </tr>
    `).join('');

    return `
      <section class="performance-section">
        <h3>最近命令记录</h3>
        <div class="performance-table-wrap">
          <table class="performance-table">
            <thead>
              <tr>
                <th>命令</th>
                <th>状态</th>
                <th>后端耗时</th>
                <th>往返耗时</th>
                <th>时间</th>
              </tr>
            </thead>
            <tbody>${rows}</tbody>
          </table>
        </div>
      </section>
    `;
  }

  function normalizeAggregates(raw) {
    const result = {};
    if (!raw || typeof raw !== 'object') return result;

    if (Array.isArray(raw)) {
      raw.forEach(item => {
        if (item && item.command) result[item.command] = normalizeAggregate(item);
      });
      return result;
    }

    Object.keys(raw).forEach(command => {
      result[command] = normalizeAggregate(raw[command]);
    });
    return result;
  }

  function normalizeAggregate(aggregate) {
    const count = Number(aggregate && aggregate.count) || 0;
    const totalMs = Number(aggregate && aggregate.totalMs) || 0;
    const avgMs = aggregate && aggregate.avgMs != null
      ? Number(aggregate.avgMs)
      : (count > 0 ? totalMs / count : 0);
    return {
      count,
      totalMs,
      avgMs,
      minMs: Number(aggregate && aggregate.minMs) || 0,
      maxMs: Number(aggregate && aggregate.maxMs) || 0,
    };
  }

  function mergeRecentRecords(backendRecent, clientRecent) {
    const clientByCommand = {};
    (Array.isArray(clientRecent) ? clientRecent : []).forEach(record => {
      if (!record || !record.command) return;
      if (!clientByCommand[record.command]) clientByCommand[record.command] = [];
      clientByCommand[record.command].push(record);
    });

    return (Array.isArray(backendRecent) ? backendRecent : []).map(record => {
      const clientList = clientByCommand[record.command] || [];
      const clientRecord = clientList.shift();
      return {
        command: record.command,
        status: record.status,
        backendMs: record.backendMs,
        roundTripMs: clientRecord ? clientRecord.roundTripMs : null,
        timestamp: clientRecord ? clientRecord.timestamp : '',
      };
    });
  }

  function totalCount(aggregates) {
    return Object.values(aggregates).reduce((sum, item) => sum + item.count, 0);
  }

  function averageTotal(aggregates) {
    const values = Object.values(aggregates);
    const count = values.reduce((sum, item) => sum + item.count, 0);
    const total = values.reduce((sum, item) => sum + item.totalMs, 0);
    return count > 0 ? total / count : 0;
  }

  function formatMs(value) {
    const number = Number(value);
    if (!Number.isFinite(number)) return '--';
    if (number < 10) return `${number.toFixed(2)} ms`;
    if (number < 100) return `${number.toFixed(1)} ms`;
    return `${Math.round(number)} ms`;
  }

  return { init, show, refresh };
})();

window.PerformanceView = PerformanceView;
