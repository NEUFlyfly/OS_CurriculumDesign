# FlyflyUFS Explorer

A Windows 11 style file explorer frontend for the FlyflyUFS custom filesystem.

## Quick Start

1. Open `index.html` in any modern browser (Chrome, Edge, Firefox)
2. The app auto-connects to mock mode — no server needed
3. Start browsing the simulated filesystem

## Connecting to Backend

The frontend automatically attempts to connect to the C++ backend via WebSocket:

- **Endpoint**: `ws://localhost:9001/ws`
- If the backend is running, the app switches from mock to real mode
- If the backend is not available, the app continues in mock mode seamlessly

## One-Click Launch

Run `ZhanGay.bat` in the project root to build, start server, and open frontend automatically.

## Features

- **File Operations**: Create, rename, delete files and folders
- **Navigation**: Back/Forward, address bar, sidebar tree, breadcrumbs
- **Views**: Grid and list view with column sorting
- **Context Menu**: Right-click menu with Windows 11 styling
- **Keyboard Shortcuts**: F2 (rename), Delete, Enter, Escape, Ctrl+A, F5, Backspace
- **Toast Notifications**: Success, error, info messages
- **Properties Dialog**: View file/folder metadata

## Project Structure

```
frontend/
├── index.html              # Main entry point
├── css/
│   └── styles.css          # Design system & all styling
├── js/
│   ├── api.js              # Mock API (simulated filesystem)
│   ├── ws-client.js        # WebSocket client for C++ backend
│   ├── app.js              # Main app state & coordination
│   ├── components/
│   │   ├── toolbar.js      # Address bar, search, actions
│   │   ├── sidebar.js      # Quick access & folder tree
│   │   ├── content.js      # Grid/list file display
│   │   ├── statusbar.js    # Item count & storage info
│   │   ├── contextmenu.js  # Right-click context menu
│   │   └── toast.js        # Toast notifications
│   └── utils/
│       ├── icons.js        # SVG icon constants
│       └── helpers.js      # Formatting, sorting, utilities
└── README.md
```

## Browser Support

- Chrome 90+
- Edge 90+
- Firefox 88+
- Safari 14+
