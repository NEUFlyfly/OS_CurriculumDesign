/**
 * FlyflyUFS Explorer — SVG Icon Constants
 * All icons as inline SVG strings. No external dependencies.
 */
const Icons = {
  // ── Navigation ──
  back: `<svg viewBox="0 0 20 20" fill="none"><path d="M12 5l-5 5 5 5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  forward: `<svg viewBox="0 0 20 20" fill="none"><path d="M8 5l5 5-5 5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  up: `<svg viewBox="0 0 20 20" fill="none"><path d="M6 12l4-4 4 4" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  down: `<svg viewBox="0 0 20 20" fill="none"><path d="M6 8l4 4 4-4" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  chevronRight: `<svg viewBox="0 0 16 16" fill="none"><path d="M6 4l4 4-4 4" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,

  // ── File/Folder Types ──
  folder: `<svg viewBox="0 0 24 24" fill="none"><path d="M2 6a2 2 0 012-2h5l2 2h9a2 2 0 012 2v10a2 2 0 01-2 2H4a2 2 0 01-2-2V6z" fill="#F6C358" stroke="#E8A817" stroke-width="1"/></svg>`,
  folderOpen: `<svg viewBox="0 0 24 24" fill="none"><path d="M2 6a2 2 0 012-2h5l2 2h9a2 2 0 012 2v2H4.5l-1.5 9V6z" fill="#F6C358" stroke="#E8A817" stroke-width="1"/><path d="M4.5 10H22l-2 10H3l1.5-10z" fill="#FDE293" stroke="#E8A817" stroke-width="1"/></svg>`,

  fileDefault: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#E0E0E0" stroke="#BDBDBD" stroke-width="1"/><path d="M14 2v6h6" fill="#F5F5F5" stroke="#BDBDBD" stroke-width="1"/></svg>`,
  fileImage: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#E74856" stroke="#C42B1C" stroke-width="1"/><path d="M14 2v6h6" fill="#FDE7E9" stroke="#C42B1C" stroke-width="1"/><circle cx="9" cy="13" r="2" fill="#FFB900"/><path d="M6 21l4-5 3 3 2-2 3 4H6z" fill="#FF8C00"/></svg>`,
  fileCode: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#0078D4" stroke="#106EBE" stroke-width="1"/><path d="M14 2v6h6" fill="#E8F4FD" stroke="#106EBE" stroke-width="1"/><path d="M8 12l2 2-2 2M16 12l-2 2 2 2" stroke="white" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  fileDoc: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#0078D4" stroke="#106EBE" stroke-width="1"/><path d="M14 2v6h6" fill="#E8F4FD" stroke="#106EBE" stroke-width="1"/><path d="M7 13h10M7 16h10M7 19h7" stroke="white" stroke-width="1.5" stroke-linecap="round"/></svg>`,
  filePdf: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#C42B1C" stroke="#A61E12" stroke-width="1"/><path d="M14 2v6h6" fill="#FDE7E9" stroke="#A61E12" stroke-width="1"/><text x="12" y="18" text-anchor="middle" fill="white" font-size="8" font-weight="700" font-family="Arial">PDF</text></svg>`,
  fileVideo: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#9B59B6" stroke="#7D3C98" stroke-width="1"/><path d="M14 2v6h6" fill="#F4ECF7" stroke="#7D3C98" stroke-width="1"/><polygon points="9,10 17,14 9,18" fill="white"/></svg>`,
  fileAudio: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#10893E" stroke="#0B6E31" stroke-width="1"/><path d="M14 2v6h6" fill="#DFF6DD" stroke="#0B6E31" stroke-width="1"/><circle cx="12" cy="14" r="4" fill="white"/><polygon points="10.5,12 10.5,16 14,14" fill="#10893E"/></svg>`,
  fileArchive: `<svg viewBox="0 0 24 24" fill="none"><path d="M6 2h8l6 6v12a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2z" fill="#CA5010" stroke="#A3440C" stroke-width="1"/><path d="M14 2v6h6" fill="#FDF1EA" stroke="#A3440C" stroke-width="1"/><rect x="10" y="11" width="4" height="2" fill="white"/><rect x="10" y="14" width="4" height="2" fill="white"/><rect x="10" y="17" width="4" height="2" fill="white"/></svg>`,

  // ── Actions ──
  newFolder: `<svg viewBox="0 0 20 20" fill="none"><path d="M2 5a1.5 1.5 0 011.5-1.5h4l1.5 1.5h7A1.5 1.5 0 0117.5 6.5v8A1.5 1.5 0 0116 16H3.5A1.5 1.5 0 012 14.5V5z" fill="#F6C358" stroke="#E8A817" stroke-width="1"/><path d="M9.5 9v5M7 11.5h5" stroke="#CA5010" stroke-width="1.5" stroke-linecap="round"/></svg>`,
  newFile: `<svg viewBox="0 0 20 20" fill="none"><path d="M5 2h6l4.5 4.5v10A1.5 1.5 0 0114 18H5a1.5 1.5 0 01-1.5-1.5V3.5A1.5 1.5 0 015 2z" fill="#E0E0E0" stroke="#BDBDBD" stroke-width="1"/><path d="M11 2v4.5h4.5" fill="#FAFAFA" stroke="#BDBDBD" stroke-width="1"/><path d="M9.5 9v5M7 11.5h5" stroke="#0078D4" stroke-width="1.5" stroke-linecap="round"/></svg>`,
  delete: `<svg viewBox="0 0 20 20" fill="none"><path d="M4.5 5h11M8.5 5V3.5a1 1 0 011-1h1a1 1 0 011 1V5M15.5 5l-.7 11.3a1 1 0 01-1 .7H6.2a1 1 0 01-1-.7L4.5 5" stroke="currentColor" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  rename: `<svg viewBox="0 0 20 20" fill="none"><path d="M13.5 3.5l3 3L7 16H4v-3l9.5-9.5z" stroke="currentColor" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  refresh: `<svg viewBox="0 0 20 20" fill="none"><path d="M17 10a7 7 0 01-7 7 7 7 0 01-7-7 7 7 0 017-7 6.95 6.95 0 015.66 3" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/><path d="M14 6h3V3" stroke="currentColor" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  search: `<svg viewBox="0 0 20 20" fill="none"><circle cx="9" cy="9" r="5.5" stroke="currentColor" stroke-width="1.25"/><path d="M13 13l4.5 4.5" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/></svg>`,
  copy: `<svg viewBox="0 0 20 20" fill="none"><rect x="5.5" y="5.5" width="9" height="11" rx="1.5" stroke="currentColor" stroke-width="1.25"/><path d="M3 14.5V4a1.5 1.5 0 011.5-1.5H15" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/></svg>`,
  paste: `<svg viewBox="0 0 20 20" fill="none"><path d="M7 3.5A1.5 1.5 0 018.5 2h3A1.5 1.5 0 0113 3.5V4h3.5A1.5 1.5 0 0118 5.5v11a1.5 1.5 0 01-1.5 1.5h-13A1.5 1.5 0 012 16.5v-11A1.5 1.5 0 013.5 4H7v-.5z" stroke="currentColor" stroke-width="1.25"/></svg>`,
  properties: `<svg viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="3" stroke="currentColor" stroke-width="1.25"/><path d="M10 2v3M10 15v3M3.5 10h3M13.5 10h3M4.93 4.93l2.12 2.12M12.95 12.95l2.12 2.12M4.93 15.07l2.12-2.12M12.95 7.05l2.12-2.12" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/></svg>`,
  open: `<svg viewBox="0 0 20 20" fill="none"><path d="M11 3h6v6M17 3L9 11" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/><path d="M17 13v3.5a1.5 1.5 0 01-1.5 1.5H4.5A1.5 1.5 0 013 16.5V5.5A1.5 1.5 0 014.5 4H8" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/></svg>`,

  // ── View Modes ──
  gridView: `<svg viewBox="0 0 20 20" fill="none"><rect x="2" y="2" width="7" height="7" rx="1" stroke="currentColor" stroke-width="1.25"/><rect x="11" y="2" width="7" height="7" rx="1" stroke="currentColor" stroke-width="1.25"/><rect x="2" y="11" width="7" height="7" rx="1" stroke="currentColor" stroke-width="1.25"/><rect x="11" y="11" width="7" height="7" rx="1" stroke="currentColor" stroke-width="1.25"/></svg>`,
  listView: `<svg viewBox="0 0 20 20" fill="none"><rect x="2" y="3" width="16" height="3" rx="1" stroke="currentColor" stroke-width="1.25"/><rect x="2" y="8.5" width="16" height="3" rx="1" stroke="currentColor" stroke-width="1.25"/><rect x="2" y="14" width="16" height="3" rx="1" stroke="currentColor" stroke-width="1.25"/></svg>`,

  // ── Quick Access ──
  desktop: `<svg viewBox="0 0 20 20" fill="none"><rect x="2" y="3" width="16" height="11" rx="1.5" stroke="currentColor" stroke-width="1.25"/><path d="M7 17h6M10 14v3" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/><rect x="5" y="17" width="10" height="1.5" rx="0.75" fill="currentColor"/></svg>`,
  documents: `<svg viewBox="0 0 20 20" fill="none"><path d="M4 3.5A1.5 1.5 0 015.5 2h5l4 4v10.5A1.5 1.5 0 0113 18H5.5A1.5 1.5 0 014 16.5v-13z" stroke="currentColor" stroke-width="1.25"/><path d="M10.5 2v4.5H15" stroke="currentColor" stroke-width="1.25"/></svg>`,
  downloads: `<svg viewBox="0 0 20 20" fill="none"><path d="M10 3v10M6 9l4 4 4-4" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/><path d="M3 14v1.5A1.5 1.5 0 004.5 17h11a1.5 1.5 0 001.5-1.5V14" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/></svg>`,
  pictures: `<svg viewBox="0 0 20 20" fill="none"><rect x="2.5" y="3.5" width="15" height="13" rx="1.5" stroke="currentColor" stroke-width="1.25"/><circle cx="7" cy="8" r="1.5" stroke="currentColor" stroke-width="1"/><path d="M2.5 13.5l4-3.5 3 2.5 2.5-2 5.5 4.5" stroke="currentColor" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  pc: `<svg viewBox="0 0 20 20" fill="none"><rect x="1" y="3" width="18" height="12" rx="1.5" stroke="currentColor" stroke-width="1.25"/><path d="M7 17h6M10 15v2" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/><rect x="5" y="17" width="10" height="1.5" rx="0.75" fill="currentColor"/></svg>`,

  // ── Status / Toast ──
  success: `<svg viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="8" stroke="currentColor" stroke-width="1.5"/><path d="M6.5 10l2.5 2.5 4.5-5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  error: `<svg viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="8" stroke="currentColor" stroke-width="1.5"/><path d="M7.5 7.5l5 5M12.5 7.5l-5 5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>`,
  info: `<svg viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="8" stroke="currentColor" stroke-width="1.5"/><path d="M10 9v5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><circle cx="10" cy="6.5" r="0.75" fill="currentColor"/></svg>`,
  warning: `<svg viewBox="0 0 20 20" fill="none"><path d="M10 2L1.5 17h17L10 2z" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/><path d="M10 8v3" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><circle cx="10" cy="14" r="0.75" fill="currentColor"/></svg>`,
  close: `<svg viewBox="0 0 16 16" fill="none"><path d="M4 4l8 8M12 4l-8 8" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>`,

  // ── Misc ──
  path: `<svg viewBox="0 0 16 16" fill="none"><path d="M2 4h3l1.5 1.5h7.5v7H2V4z" fill="#F6C358" stroke="#E8A817" stroke-width="0.75"/></svg>`,
  storage: `<svg viewBox="0 0 16 16" fill="none"><rect x="1.5" y="2.5" width="13" height="11" rx="1.5" stroke="currentColor" stroke-width="1"/><path d="M5 11.5h6" stroke="currentColor" stroke-width="1" stroke-linecap="round"/></svg>`,
  emptyFolder: `<svg viewBox="0 0 24 24" fill="none"><path d="M2 6a2 2 0 012-2h5l2 2h9a2 2 0 012 2v10a2 2 0 01-2 2H4a2 2 0 01-2-2V6z" fill="#F0F0F0" stroke="#D0D0D0" stroke-width="1"/></svg>`,

  // ── Context Menu ──
  menuOpen: `<svg viewBox="0 0 16 16" fill="none"><path d="M6 3h7M6 8h7M6 13h7" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"/><circle cx="3" cy="3" r="1.25" fill="currentColor"/><circle cx="3" cy="8" r="1.25" fill="currentColor"/><circle cx="3" cy="13" r="1.25" fill="currentColor"/></svg>`,
};

/**
 * Map file extensions to their icon type.
 * Returns the appropriate icon SVG string for a given filename.
 */
function getFileIcon(filename) {
  if (!filename || filename === '..') return Icons.folder;

  const ext = filename.split('.').pop().toLowerCase();

  const imageExts = ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'svg', 'webp', 'ico', 'tiff', 'tif'];
  const codeExts  = ['js', 'ts', 'jsx', 'tsx', 'py', 'cpp', 'c', 'h', 'hpp', 'java', 'rs', 'go', 'rb', 'php', 'swift', 'kt', 'cs', 'css', 'html', 'htm', 'xml', 'json', 'yaml', 'yml', 'toml', 'sh', 'bash', 'ps1', 'bat', 'cmd'];
  const docExts   = ['txt', 'md', 'rtf', 'log', 'csv', 'tex'];
  const pdfExts   = ['pdf'];
  const videoExts = ['mp4', 'avi', 'mkv', 'mov', 'wmv', 'flv', 'webm', 'm4v'];
  const audioExts = ['mp3', 'wav', 'flac', 'aac', 'ogg', 'wma', 'm4a'];
  const archiveExts = ['zip', 'rar', '7z', 'tar', 'gz', 'bz2', 'xz', 'iso'];

  if (imageExts.includes(ext)) return Icons.fileImage;
  if (codeExts.includes(ext))  return Icons.fileCode;
  if (docExts.includes(ext))   return Icons.fileDoc;
  if (pdfExts.includes(ext))   return Icons.filePdf;
  if (videoExts.includes(ext)) return Icons.fileVideo;
  if (audioExts.includes(ext)) return Icons.fileAudio;
  if (archiveExts.includes(ext)) return Icons.fileArchive;

  return Icons.fileDefault;
}

/**
 * Get icon for a file system entry.
 * @param {Object} item - { name, type } 
 */
function getEntryIcon(item) {
  if (item.type === 'dir') {
    return Icons.folder;
  }
  return getFileIcon(item.name);
}

// Make available globally for non-module scripts
window.Icons = Icons;
window.getFileIcon = getFileIcon;
window.getEntryIcon = getEntryIcon;
