/**
 * FlyflyUFS Explorer - Storage Inspection View
 * Renders real backend storage metadata from the storage_inspection command.
 */
const StorageView = (function () {
  'use strict';

  const views = {
    'storage-whole-disk': {
      title: '整盘块布局',
      description: '按整个 data.img 的逻辑块编号展示 SuperBlock、位图、inode 表和数据区。',
      layout: 'whole-disk',
    },
    'storage-metadata-layout': {
      title: '元数据区详情',
      description: '展示元数据区域在磁盘中的起止块、字节范围和用途。',
      layout: 'metadata',
    },
    'storage-disk-blocks': {
      title: '磁盘块占用情况',
      description: '按数据块编号显示 block bitmap 中的真实占用状态。',
      bitmap: 'blocks',
      cellLabel: '数据块',
    },
    'storage-block-bitmap': {
      title: 'Block 位图',
      description: '1 表示已占用，0 表示空闲。红色为占用，绿色为空闲。',
      bitmap: 'blocks',
      cellLabel: 'Block',
    },
    'storage-inode-bitmap': {
      title: 'Inode 位图',
      description: '1 表示 inode 已分配，0 表示 inode 空闲。',
      bitmap: 'inodes',
      cellLabel: 'Inode',
    },
    'storage-inode-table': {
      title: 'Inode 表',
      description: '逐项显示 inode 表中的分配状态和已用 inode 元数据。',
    },
    'storage-grouped-linking': {
      title: '成组链接法示意图',
      description: '根据当前 SuperBlock 空闲块栈和磁盘上的后续组索引块绘制真实状态。',
      schematic: 'grouped-linking',
    },
  };

  let container = null;
  let callbacks = {};
  let currentView = 'storage-disk-blocks';
  let cachedSnapshot = null;
  let groupedStackGroups = [];
  let groupedStackCache = new Map();

  function init(parentEl, cb) {
    container = parentEl;
    callbacks = cb || {};
  }

  async function show(viewKey) {
    currentView = views[viewKey] ? viewKey : 'storage-disk-blocks';

    if (cachedSnapshot) {
      render(cachedSnapshot);
      return;
    }

    renderLoading();

    try {
      const snapshot = await callbacks.fetchSnapshot();
      cachedSnapshot = snapshot;
      render(snapshot);
    } catch (e) {
      renderError(e);
    }
  }

  function refresh() {
    invalidateCache();
    return show(currentView);
  }

  function invalidateCache() {
    cachedSnapshot = null;
    groupedStackGroups = [];
    groupedStackCache.clear();
  }

  function renderLoading() {
    if (!container) return;
    container.innerHTML = '<div class="storage-view loading-state">正在读取文件系统结构...</div>';
  }

  function renderError(error) {
    if (!container) return;
    const message = error && error.message ? error.message : '无法读取存储结构';
    container.innerHTML = `
      <div class="storage-view storage-error">
        <div class="storage-title">读取失败</div>
        <div class="storage-description">${Helpers.escapeHtml(message)}</div>
      </div>
    `;
  }

  function render(snapshot) {
    if (!container) return;
    const config = views[currentView];
    container.innerHTML = `
      <div class="storage-view">
        <div class="storage-header">
          <div>
            <div class="storage-title">${Helpers.escapeHtml(config.title)}</div>
            <div class="storage-description">${Helpers.escapeHtml(config.description)}</div>
          </div>
          ${renderHeaderLegend(config)}
        </div>
        ${renderSummary(snapshot)}
        <div class="storage-content">
          ${renderStorageContent(snapshot, config)}
        </div>
      </div>
    `;

    if (config.schematic === 'grouped-linking') {
      bindGroupedStackDetails();
    }
  }

  function renderStorageContent(snapshot, config) {
    if (config.schematic === 'grouped-linking') {
      return renderGroupedLinkingDiagram(snapshot);
    }
    if (config.layout === 'whole-disk') {
      return renderWholeDiskLayout(snapshot);
    }
    if (config.layout === 'metadata') {
      return renderMetadataLayout(snapshot);
    }
    return currentView === 'storage-inode-table' ? renderInodeTable(snapshot) : renderBitmap(snapshot, config);
  }

  function renderHeaderLegend(config) {
    if (config.schematic || config.layout === 'metadata' || currentView === 'storage-inode-table') {
      return '';
    }
    if (config.layout === 'whole-disk') {
      return renderDiskLegend();
    }
    return renderLegend();
  }

  function renderLegend() {
    return `
      <div class="storage-legend" aria-label="颜色说明">
        <span><i class="storage-swatch is-used"></i>已占用</span>
        <span><i class="storage-swatch is-free"></i>空闲</span>
      </div>
    `;
  }

  function renderDiskLegend() {
    return `
      <div class="storage-legend storage-legend-disk" aria-label="整盘颜色说明">
        <span><i class="storage-swatch is-reserved"></i>SuperBlock</span>
        <span><i class="storage-swatch is-metadata-used"></i>元数据占用</span>
        <span><i class="storage-swatch is-metadata-partial"></i>inode 表部分占用</span>
        <span><i class="storage-swatch is-metadata-empty"></i>inode 表空闲</span>
        <span><i class="storage-swatch is-data-used"></i>数据块占用</span>
        <span><i class="storage-swatch is-data-free"></i>数据块空闲</span>
      </div>
    `;
  }

  function renderSummary(snapshot) {
    const layout = snapshot.layout || {};
    const summary = snapshot.summary || {};
    const totalBlocks = numberOrDash(layout.totalBlocks);
    const totalInodes = numberOrDash(layout.totalInodes);
    const usedBlocks = numberOrDash(summary.usedBlocks);
    const freeBlocks = numberOrDash(summary.freeBlocks);
    const usedInodes = numberOrDash(summary.usedInodes);
    const freeInodes = numberOrDash(summary.freeInodes);

    return `
      <div class="storage-summary">
        <div class="storage-card">
          <span>块大小</span>
          <strong>${numberOrDash(layout.blockSize)} B</strong>
        </div>
        <div class="storage-card">
          <span>数据块</span>
          <strong>${usedBlocks} / ${totalBlocks}</strong>
          <em>${freeBlocks} 空闲</em>
        </div>
        <div class="storage-card">
          <span>Inode</span>
          <strong>${usedInodes} / ${totalInodes}</strong>
          <em>${freeInodes} 空闲</em>
        </div>
        <div class="storage-card">
          <span>数据区起始</span>
          <strong>${numberOrDash(layout.dataBlockStart)}</strong>
        </div>
      </div>
    `;
  }

  function renderBitmap(snapshot, config) {
    const bitmaps = snapshot.bitmaps || {};
    const values = Array.isArray(bitmaps[config.bitmap]) ? bitmaps[config.bitmap] : [];

    if (values.length === 0) {
      return '<div class="storage-empty">后端没有返回位图数据</div>';
    }

    const used = values.filter(Boolean).length;
    const free = values.length - used;
    const cells = values.map((value, index) => {
      const occupied = value === 1 || value === true;
      const status = occupied ? '已占用' : '空闲';
      const className = occupied ? 'is-used' : 'is-free';
      const title = `${config.cellLabel} #${index}: ${status}`;
      const detail = renderCellHoverPanel(snapshot, config, index, occupied);
      return `
        <span class="storage-cell storage-cell-hover ${className}" data-index="${index}" title="${title}" aria-label="${title}" tabindex="0">
          ${detail}
        </span>
      `;
    }).join('');

    return `
      <div class="storage-counts">总数 ${values.length}，已占用 ${used}，空闲 ${free}</div>
      <div class="storage-grid" role="img" aria-label="${Helpers.escapeHtml(config.title)}红绿占用图">
        ${cells}
      </div>
    `;
  }

  function renderInodeTable(snapshot) {
    const rows = Array.isArray(snapshot.inodeTable) ? snapshot.inodeTable : [];

    if (rows.length === 0) {
      return '<div class="storage-empty">后端没有返回 inode 表数据</div>';
    }

    const body = rows.map(inode => {
      const used = inode.used === true;
      const status = used ? '已占用' : '空闲';
      const blocks = Array.isArray(inode.directBlocks) && inode.directBlocks.length > 0
        ? inode.directBlocks.join(', ')
        : '-';
      return `
        <tr class="${used ? 'is-used' : 'is-free'}">
          <td>${numberOrDash(inode.index)}</td>
          <td>${numberOrDash(inode.address)}</td>
          <td><span class="storage-badge ${used ? 'is-used' : 'is-free'}">${status}</span></td>
          <td>${used ? Helpers.escapeHtml(inode.type || '-') : '-'}</td>
          <td>${used ? Helpers.escapeHtml(inode.permissions || '-') : '-'}</td>
          <td>${used ? Helpers.escapeHtml(inode.owner || '-') : '-'}</td>
          <td>${used ? Helpers.escapeHtml(inode.group || '-') : '-'}</td>
          <td>${used ? numberOrDash(inode.size) : '-'}</td>
          <td>${used ? numberOrDash(inode.linkCount) : '-'}</td>
          <td>${used ? Helpers.escapeHtml(blocks) : '-'}</td>
        </tr>
      `;
    }).join('');

    return `
      <div class="storage-table-wrap">
        <table class="storage-table">
          <thead>
            <tr>
              <th>#</th>
              <th>地址</th>
              <th>状态</th>
              <th>类型</th>
              <th>权限</th>
              <th>用户</th>
              <th>组</th>
              <th>大小</th>
              <th>链接</th>
              <th>直接块</th>
            </tr>
          </thead>
          <tbody>${body}</tbody>
        </table>
      </div>
    `;
  }

  function renderWholeDiskLayout(snapshot) {
    const diskLayout = snapshot.diskLayout || {};
    const blocks = Array.isArray(diskLayout.blocks) ? diskLayout.blocks : [];

    if (blocks.length === 0) {
      return '<div class="storage-empty">后端没有返回整盘块布局数据</div>';
    }

    const metadataBlocks = numberOrDash(diskLayout.metadataBlocks);
    const dataBlocks = numberOrDash(diskLayout.dataBlocks);
    const totalBlocks = numberOrDash(diskLayout.totalBlocks);
    const cells = blocks.map(block => {
      const state = stateLabel(block.state);
      const className = stateToClass(block.state);
      const title = `整盘块 #${numberOrDash(block.index)}: ${block.regionLabel || block.region || state}`;
      return `
        <span class="storage-cell storage-cell-hover ${className}" data-index="${numberOrDash(block.index)}" title="${Helpers.escapeHtml(title)}" aria-label="${Helpers.escapeHtml(title)}" tabindex="0">
          ${renderHoverPanel('整盘块详情', getDiskBlockDetailRows(block))}
        </span>
      `;
    }).join('');

    return `
      <div class="storage-counts">整盘逻辑块 ${totalBlocks}：元数据块 ${metadataBlocks}，数据块 ${dataBlocks}</div>
      <div class="storage-grid storage-grid-disk" role="img" aria-label="整盘块布局图">
        ${cells}
      </div>
    `;
  }

  function renderMetadataLayout(snapshot) {
    const diskLayout = snapshot.diskLayout || {};
    const regions = Array.isArray(diskLayout.regions) ? diskLayout.regions : [];

    if (regions.length === 0) {
      return '<div class="storage-empty">后端没有返回元数据区域信息</div>';
    }

    const cards = regions.map(region => `
      <section class="metadata-region-card ${stateToClass(region.state)}">
        <div class="metadata-region-title">${Helpers.escapeHtml(region.label || region.id || '--')}</div>
        <div class="metadata-region-description">${Helpers.escapeHtml(region.description || '')}</div>
        <dl class="metadata-region-fields">
          <div><dt>区域</dt><dd>${Helpers.escapeHtml(region.id || '--')}</dd></div>
          <div><dt>类型</dt><dd>${Helpers.escapeHtml(region.kind || '--')}</dd></div>
          <div><dt>状态</dt><dd>${stateLabel(region.state)}</dd></div>
          <div><dt>起始块</dt><dd>${numberOrDash(region.startBlock)}</dd></div>
          <div><dt>块数</dt><dd>${numberOrDash(region.blockCount)}</dd></div>
          <div><dt>字节范围</dt><dd>${numberOrDash(region.startByte)} - ${numberOrDash(region.endByte)}</dd></div>
        </dl>
      </section>
    `).join('');

    return `
      <div class="metadata-layout-note">
        block bitmap 只记录数据区 block 的占用；这里展示的是 data.img 从 0 开始的完整逻辑块布局。
      </div>
      <div class="metadata-region-grid">${cards}</div>
    `;
  }

  function renderGroupedLinkingDiagram(snapshot) {
    const grouped = snapshot.groupedLinking || {};
    const groups = Array.isArray(grouped.groups) ? grouped.groups : [];

    if (groups.length === 0) {
      return '<div class="storage-empty">后端没有返回成组链接法数据</div>';
    }

    groupedStackGroups = groups;
    const groupNodes = groups.map((group, index) => renderGroupedLinkingGroup(group, index)).join('');
    const firstGroup = groups[0] || {};

    return `
      <div class="storage-schematic" aria-label="当前磁盘成组链接法状态">
        <div class="storage-schematic-overview">
          <div><span>空闲块总数</span><strong>${numberOrDash(grouped.freeBlockCount)}</strong></div>
          <div><span>每组容量</span><strong>${numberOrDash(grouped.blocksPerGroup)}</strong></div>
          <div><span>当前栈顶 top</span><strong>${numberOrDash(grouped.currentTop)}</strong></div>
          <div><span>当前组索引块</span><strong>${formatBlockRef(grouped.currentGroupBlockIndex, grouped.currentGroupBlockAddress)}</strong></div>
        </div>

        <div class="storage-schematic-steps">
          <div><strong>当前 SuperBlock 栈</strong><span>s_free_addr = ${formatBlockRef(grouped.currentGroupBlockIndex, grouped.currentGroupBlockAddress)}；s_free[0] 指向 ${formatBlockRef(firstGroup.nextGroupIndex, firstGroup.nextGroupAddress)}。</span></div>
          <div><strong>分配顺序</strong><span>先弹出当前组中 top 指向的普通空闲块；top 到 0 时分配 s_free_addr，并读取 s_free[0] 指向的下一组。</span></div>
        </div>

        <div class="storage-schematic-flow">
          ${groupNodes}
        </div>

        ${grouped.truncated ? '<p class="storage-schematic-note is-warning">检测到组链循环或读取中断，图中只展示可安全遍历的部分。</p>' : ''}
        <p class="storage-schematic-note">该图来自当前后端 storage_inspection 快照；块号 Bn 表示数据区第 n 个 block。</p>
      </div>
    `;
  }

  function renderGroupedLinkingGroup(group, stackIndex) {
    const source = group.source === 'SuperBlock' ? 'SuperBlock 当前组' : `磁盘索引组 #${numberOrDash(group.order)}`;
    const freeBlockCount = Number.isFinite(Number(group.freeBlockCount)) ? Number(group.freeBlockCount) : 0;
    const stackLength = Number.isFinite(Number(group.stackLength)) ? Number(group.stackLength) : 0;
    const firstFreeBlock = numberOrDash(group.firstFreeBlockIndex) === '--' || Number(group.firstFreeBlockIndex) < 0
      ? '无'
      : `B${numberOrDash(group.firstFreeBlockIndex)}`;
    const lastFreeBlock = numberOrDash(group.lastFreeBlockIndex) === '--' || Number(group.lastFreeBlockIndex) < 0
      ? '无'
      : `B${numberOrDash(group.lastFreeBlockIndex)}`;

    return `
      <section class="storage-schematic-node ${group.source === 'SuperBlock' ? 'is-superblock' : 'is-index'}">
        <div class="schematic-node-title">${Helpers.escapeHtml(source)}</div>
        <div class="schematic-node-subtitle">
          本组索引块 ${formatBlockRef(group.groupBlockIndex, group.groupBlockAddress)}，下一组 ${formatBlockRef(group.nextGroupIndex, group.nextGroupAddress)}
        </div>
        <div class="schematic-group-summary">
          <span>本组空闲块：${numberOrDash(freeBlockCount)} 个</span>
          <span>首块：${firstFreeBlock}</span>
          <span>末块：${lastFreeBlock}</span>
        </div>
        <details class="schematic-stack-detail" data-stack-index="${stackIndex}">
          <summary>查看本组 s_free[] 栈内容（${stackLength} 项）</summary>
          <div class="schematic-stack schematic-stack-placeholder" data-rendered="false">
            <span>展开后只加载本组 s_free[] 内容</span>
          </div>
        </details>
      </section>
    `;
  }

  function bindGroupedStackDetails() {
    const detailsList = container.querySelectorAll('.schematic-stack-detail');
    detailsList.forEach(details => {
      details.addEventListener('toggle', () => {
        if (!details.open) return;

        const placeholder = details.querySelector('.schematic-stack');
        if (!placeholder || placeholder.dataset.rendered === 'true') return;

        const stackIndex = Number(details.dataset.stackIndex);
        const group = Number.isInteger(stackIndex) ? groupedStackGroups[stackIndex] : null;
        loadGroupedStackRows(group, placeholder);
      });
    });
  }

  async function loadGroupedStackRows(group, placeholder) {
    if (!group || typeof callbacks.fetchGroupedStack !== 'function') {
      placeholder.innerHTML = '<span>无法读取本组 s_free[] 内容</span>';
      placeholder.dataset.rendered = 'true';
      return;
    }

    const cacheKey = `${group.source || ''}:${Number(group.groupBlockAddress)}`;
    if (groupedStackCache.has(cacheKey)) {
      placeholder.innerHTML = renderGroupedStackRows(groupedStackCache.get(cacheKey));
      placeholder.dataset.rendered = 'true';
      placeholder.classList.remove('schematic-stack-placeholder');
      return;
    }

    placeholder.innerHTML = '<span>正在读取本组 s_free[] 内容...</span>';

    try {
      const stack = await callbacks.fetchGroupedStack(group);
      groupedStackCache.set(cacheKey, stack);
      placeholder.innerHTML = renderGroupedStackRows(stack);
      placeholder.dataset.rendered = 'true';
      placeholder.classList.remove('schematic-stack-placeholder');
    } catch (e) {
      const message = e && e.message ? e.message : '读取 s_free[] 失败';
      placeholder.innerHTML = `<span>${Helpers.escapeHtml(message)}</span>`;
    }
  }

  function renderGroupedStackRows(stack) {
    if (!Array.isArray(stack) || stack.length === 0) {
      return '<span>无栈内容</span>';
    }

    return stack.map((address, index) => {
      const role = index === 0 ? '下一组指针' : '空闲块';
      return `<span>s_free[${index}] ${role}: ${formatBlockRef(-1, address)}</span>`;
    }).join('');
  }

  function formatBlockRef(index, address) {
    const blockIndex = index === null || index === undefined ? NaN : Number(index);
    const blockAddress = Number(address);
    if (Number.isFinite(blockIndex) && blockIndex >= 0) {
      return `B${blockIndex}${Number.isFinite(blockAddress) && blockAddress >= 0 ? `（${blockAddress}）` : ''}`;
    }
    if (Number.isFinite(blockAddress) && blockAddress >= 0) {
      return `地址 ${blockAddress}`;
    }
    return '无';
  }

  function renderCellHoverPanel(snapshot, config, index, occupied) {
    const detailRows = getBitmapCellDetailRows(snapshot, config, index, occupied);
    const title = config.bitmap === 'inodes' ? 'iNode 结构体' : 'Block 位图项';
    return renderHoverPanel(title, detailRows);
  }

  function getBitmapCellDetailRows(snapshot, config, index, occupied) {
    if (config.bitmap === 'inodes') {
      const inodeTable = Array.isArray(snapshot.inodeTable) ? snapshot.inodeTable : [];
      const inode = inodeTable[index] || {};
      return getInodeDetailRows(inode.inodeDetail || {});
    }

    const layout = snapshot.layout || {};
    const blockSize = Number(layout.blockSize);
    const dataBlockStart = Number(layout.dataBlockStart);
    const address = Number.isFinite(blockSize) && Number.isFinite(dataBlockStart)
      ? dataBlockStart + index * blockSize
      : null;

    return [
      ['block_index', index],
      ['bitmap_value', occupied ? 1 : 0],
      ['status', occupied ? '已占用' : '空闲'],
      ['block_address', address],
    ];
  }

  function getDiskBlockDetailRows(block) {
    const rows = [
      ['block_index', block.index],
      ['byte_offset', block.byteOffset],
      ['region', block.region],
      ['region_label', block.regionLabel],
      ['kind', block.kind],
      ['state', stateLabel(block.state)],
    ];

    if (block.dataBlockIndex !== null && block.dataBlockIndex !== undefined && Number(block.dataBlockIndex) >= 0) {
      rows.push(['data_block_index', block.dataBlockIndex]);
      rows.push(['data_block_address', block.dataBlockAddress]);
    }
    if (block.usedRecords !== null && block.usedRecords !== undefined && Number(block.totalRecords) > 0) {
      rows.push(['inode_records', `${numberOrDash(block.usedRecords)} / ${numberOrDash(block.totalRecords)}`]);
    }

    return rows;
  }

  function stateToClass(state) {
    switch (state) {
      case 'reserved':
        return 'is-reserved';
      case 'metadata-used':
        return 'is-metadata-used';
      case 'metadata-partial':
        return 'is-metadata-partial';
      case 'metadata-empty':
        return 'is-metadata-empty';
      case 'data-used':
        return 'is-data-used';
      case 'data-free':
        return 'is-data-free';
      default:
        return 'is-metadata-empty';
    }
  }

  function stateLabel(state) {
    switch (state) {
      case 'reserved':
        return '保留结构';
      case 'metadata-used':
        return '元数据占用';
      case 'metadata-partial':
        return '部分占用';
      case 'metadata-empty':
        return '空元数据块';
      case 'data-used':
        return '数据块占用';
      case 'data-free':
        return '数据块空闲';
      default:
        return state || '--';
    }
  }

  function renderHoverPanel(title, detailRows) {
    const rows = detailRows.map(([label, value]) => `
      <div class="storage-hover-row">
        <dt>${Helpers.escapeHtml(label)}</dt>
        <dd>${formatDetailValue(value)}</dd>
      </div>
    `).join('');

    return `
      <span class="storage-hover-panel" role="tooltip">
        <span class="storage-hover-title">${Helpers.escapeHtml(title)}</span>
        <dl>${rows}</dl>
      </span>
    `;
  }

  function getInodeDetailRows(detail) {
    return [
      ['inode_id', detail.inode_id],
      ['inode_mode', detail.inode_mode],
      ['inode_cnt', detail.inode_cnt],
      ['user_name', detail.user_name],
      ['user_group', detail.user_group],
      ['inode_size', detail.inode_size],
      ['inode_dirblock[10]', detail.inode_dirblock],
      ['inode_indirect_block_first', detail.inode_indirect_block_first],
    ];
  }

  function formatDetailValue(value) {
    if (Array.isArray(value)) {
      return Helpers.escapeHtml(value.length > 0 ? value.join(', ') : '--');
    }
    if (value === null || value === undefined || value === '') {
      return '--';
    }
    return Helpers.escapeHtml(String(value));
  }

  function numberOrDash(value) {
    return Number.isFinite(Number(value)) ? String(value) : '--';
  }

  return { init, show, refresh, invalidateCache };
})();

window.StorageView = StorageView;
