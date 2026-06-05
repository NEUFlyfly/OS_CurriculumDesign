# WebSocket Protocol Specification

Communication protocol between the Tauri frontend and C++ filesystem backend over WebSocket.

## 1. Transport

- **Transport**: WebSocket (RFC 6455)
- **Message format**: JSON text frames
- **Encoding**: UTF-8
- **Endpoint**: `ws://localhost:<port>/ws` (exact port TBD at implementation)

## 2. Message Envelope

All messages follow a common envelope structure with a `type` discriminator field.

### Request (Client → Server)

```json
{
  "type": "request",
  "id": 1,
  "command": "ls",
  "params": {}
}
```

| Field      | Type   | Required | Description                                      |
|------------|--------|----------|--------------------------------------------------|
| `type`     | string | yes      | Always `"request"`                               |
| `id`       | number | yes      | Monotonic integer for request/response correlation |
| `command`  | string | yes      | Command name (see §4)                            |
| `params`   | object | yes      | Command-specific parameters (may be `{}`)        |

### Success Response (Server → Client)

```json
{
  "type": "response",
  "id": 1,
  "status": 200,
  "data": { ... }
}
```

| Field    | Type   | Required | Description                          |
|----------|--------|----------|--------------------------------------|
| `type`   | string | yes      | Always `"response"`                  |
| `id`     | number | yes      | Matches the request `id`             |
| `status` | number | yes      | HTTP-style status code (see §3)      |
| `data`   | object | yes      | Command-specific response payload    |

### Error Response (Server → Client)

```json
{
  "type": "error",
  "id": 1,
  "code": 1001,
  "message": "Unknown command: foo"
}
```

| Field     | Type   | Required | Description                                     |
|-----------|--------|----------|-------------------------------------------------|
| `type`    | string | yes      | Always `"error"`                                |
| `id`      | number | yes      | Matches the request `id` (0 if unparseable)     |
| `code`    | number | yes      | Application error code (see §3)                 |
| `message` | string | yes      | Human-readable error description                |

## 3. Status & Error Codes

### HTTP-Style Status Codes

| Code | Name         | Usage                                          |
|------|--------------|-------------------------------------------------|
| 200  | OK           | Request succeeded                               |
| 400  | Bad Request  | Malformed JSON or missing required fields       |
| 401  | Unauthorized | Not logged in or invalid credentials            |
| 403  | Forbidden    | Logged in but insufficient permissions          |
| 404  | Not Found    | Target file or directory does not exist         |
| 500  | Server Error | Internal filesystem error                       |

### Application Error Codes

| Code  | Name            | Description                                      |
|-------|-----------------|--------------------------------------------------|
| 1001  | Unknown Command | Command string is not recognized                 |
| 1002  | Invalid Params  | Required parameter is missing or malformed       |
| 1003  | Not Found       | Target file or directory does not exist          |
| 1004  | Already Exists  | File or directory already exists                 |
| 1005  | Permission Denied | Operation not permitted for current user       |
| 1006  | Filesystem Error | Filesystem not initialized or image corrupt     |
| 1007  | Name Too Long   | Name exceeds 28-character limit (MAX_NAME_SIZE)  |
| 1008  | Dir Not Empty   | Cannot remove non-empty directory                |

## 4. Commands

### 4.1 `login`

Authenticate a user.

**Params:**
```json
{ "username": "root", "password": "123456" }
```

**Response (200):**
```json
{ "username": "root", "group": "root" }
```

---

### 4.2 `ls`

List contents of the current directory.

**Params:** `{}` (none)

**Response (200):**
```json
[
  {
    "name": "documents",
    "type": "dir",
    "size": 128,
    "permissions": "0755",
    "inodeAddr": 11776,
    "owner": "root",
    "group": "root"
  },
  {
    "name": "readme.txt",
    "type": "file",
    "size": 1024,
    "permissions": "0664",
    "inodeAddr": 11904,
    "owner": "root",
    "group": "root"
  }
]
```

---

### 4.3 `cd`

Change current directory.

**Params:**
```json
{ "name": "documents" }
```

**Response (200):**
```json
{ "path": "/root/documents" }
```

---

### 4.4 `mkdir`

Create a new directory.

**Params:**
```json
{ "name": "new_folder" }
```

**Response (200):** `null` (no data payload)

---

### 4.5 `rmdir`

Remove an empty directory.

**Params:**
```json
{ "name": "old_folder" }
```

**Response (200):** `null`

---

### 4.6 `touch`

Create an empty file.

**Params:**
```json
{ "name": "file.txt" }
```

**Response (200):** `null`

---

### 4.7 `rm`

Delete a file.

**Params:**
```json
{ "name": "file.txt" }
```

**Response (200):** `null`

---

### 4.8 `cat`

Read file content.

**Params:**
```json
{ "name": "readme.txt" }
```

**Response (200):**
```json
{ "content": "Hello, world!\nThis is the file content." }
```

---

### 4.9 `chmod`

Change file/directory permissions.

**Params:**
```json
{ "name": "file.txt", "mode": "0644" }
```

**Response (200):** `null`

---

### 4.10 `pwd`

Get current working directory path.

**Params:** `{}` (none)

**Response (200):**
```json
{ "path": "/root" }
```

---

### 4.11 `useradd`

Add a new user account.

**Params:**
```json
{ "username": "alice" }
```

**Response (200):** `null`

---

### 4.12 `userdel`

Delete a user account.

**Params:**
```json
{ "username": "alice" }
```

**Response (200):** `null`

---

### 4.13 `disk_usage`

Get disk usage statistics.

**Params:** `{}` (none)

**Response (200):**
```json
{
  "totalBlocks": 10240,
  "usedBlocks": 256,
  "freeBlocks": 9984,
  "totalInodes": 640,
  "usedInodes": 32,
  "freeInodes": 608,
  "blockSize": 512
}
```

---

### 4.14 `directory_sizes`

Get size of each top-level directory (for visualization).

**Params:** `{}` (none)

**Response (200):**
```json
[
  { "name": "documents", "size": 4096, "childCount": 3 },
  { "name": "images", "size": 8192, "childCount": 5 }
]
```

## 5. Session Lifecycle

1. Client connects to WebSocket endpoint.
2. All commands except `login` return **401 Unauthorized** until the client sends a successful `login`.
3. The server maintains session state (current directory, logged-in user) per WebSocket connection.
4. Connection close terminates the session.

## 6. TypeScript Types

Full TypeScript type definitions are in:

```
frontend/src/types/websocket.ts
```

Key types: `WsRequest`, `WsResponse`, `WsError`, `CommandName`, `CommandParams`, `CommandResponses`, `FileInfo`, `DiskUsageInfo`, `DirectoryInfo`.
