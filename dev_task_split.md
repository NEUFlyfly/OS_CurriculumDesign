# FlyflyUFS 协作开发分工文档

本文档用于说明 FlyflyUFS 项目在四人协作开发场景下的模块划分、职责边界、接口约定和集成流程。项目选题为“模拟实现 UNIX 文件系统”，整体目标是在 Windows/MSYS2 环境下实现一个基于 `data.img` 虚拟磁盘文件的类 UNIX 文件系统。

## 1. 项目总体结构

FlyflyUFS 采用 C++17 和 CMake 构建。项目核心由 `FileSystem` 统一对外提供接口，内部按照文件系统生命周期、磁盘空间管理、目录管理、文件管理、用户管理、命令交互和编辑器适配等职责拆分。

主要目录如下：

```text
include/   头文件、磁盘结构定义、文件系统状态定义
src/       C++ 源文件，各模块具体实现
scripts/   自动化冒烟测试脚本
.vscode/   VS Code 一键构建、运行、调试配置
```

项目对外运行入口位于 `main.cpp`，核心公共接口位于 `include/filesystem.h`。为了保证协作时接口稳定，四名成员都应遵守以下原则：

- 不随意修改 `iNode`、`Dir`、`SuperBlock` 的结构体字段顺序和大小。
- 不随意修改 `include/const.h` 中的磁盘布局常量。
- 不随意修改 `data.img` 的二进制组织方式。
- 不随意修改命令名称、参数格式和已有输出文本。
- 新增或修改功能后必须通过构建和冒烟测试。

## 2. 四人模块分工

### 成员 A：虚拟磁盘与空间管理模块

成员 A 负责文件系统最底层的存储结构、虚拟磁盘访问和空闲空间管理。该部分是其他模块的基础，重点是保证 `data.img` 的读写稳定、超级块和位图信息正确。

负责文件：

```text
include/const.h
include/image.h
include/superblock.h
include/inode.h
include/dir.h
include/filesystem_storage.h
src/image.cpp
src/filesystem_core.cpp
src/filesystem_lifecycle.cpp
src/filesystem_alloc.cpp
```

主要职责：

- 维护虚拟磁盘文件 `data.img` 的创建、打开和关闭逻辑。
- 维护超级块 `SuperBlock`、inode 区、数据块区、位图区的组织方式。
- 实现文件系统格式化和启动加载流程。
- 实现 inode 分配、inode 回收、数据块分配、数据块回收。
- 保证 `Format()` 后能够创建根目录、`/home/root`、`/etc/user`、`/etc/passwd` 和 `/etc/group`。

对外协作接口：

- 为目录模块和文件模块提供 `INodeAlloc()`、`BlockAlloc()`、`INodeFree()`、`FreeBlock()`。
- 为所有模块提供稳定的 `storage.image`、`storage.superBlock`、`storage.inode_bitmap` 和 `storage.block_bitmap`。

验收重点：

- 删除临时目录中的 `data.img` 后首次启动能自动格式化。
- 退出后再次启动能复用已有 `data.img`。
- 创建和删除文件、目录后空闲 inode 和 block 能正确更新。

### 成员 B：目录、路径与权限模块

成员 B 负责类 UNIX 文件系统中的目录树、路径跳转、目录显示、目录权限和目录删除逻辑。该部分直接决定 `cd`、`ls`、`mkdir`、`rmdir`、`chmod` 等命令是否符合预期。

负责文件：

```text
include/filesystem_state.h
src/filesystem_path.cpp
src/filesystem_permissions.cpp
src/filesystem_listing.cpp
src/filesystem_directory_ops.cpp
```

主要职责：

- 维护当前用户所在目录地址和目录路径显示。
- 实现 `cd`、`.`、`..` 等目录跳转逻辑。
- 实现 `mkdir` 创建目录，并维护目录项中的 `.` 和 `..`。
- 实现 `ls` 目录列表输出。
- 实现 `chmod` 权限修改逻辑。
- 实现 `rmdir` 和递归目录删除。
- 维护目录访问权限、读权限、写权限和执行权限判断。

对外协作接口：

- 为文件模块提供目录项查找和父目录定位能力。
- 为用户模块提供创建用户家目录、删除用户家目录的能力。
- 为命令模块提供 `FindDir()`、`MakeDir()`、`ShowDir()`、`DeleteDir()`、`Chmod()` 等命令处理入口。

验收重点：

- `mkdir d` 后 `ls` 能显示目录。
- `cd d` 后 `pwd` 能显示 `/home/root/d`。
- `cd ..` 能回到父目录。
- `rmdir d` 能删除目录。
- 权限不足时应保持原有提示和行为。

### 成员 C：文件、编辑器与命令交互模块

成员 C 负责普通文件的创建、删除、读取、写入，以及命令行交互和 ncurses 编辑器适配。该部分是用户直接操作文件内容的主要入口。

负责文件：

```text
include/editor.h
src/editor.cpp
src/filesystem_file_create.cpp
src/filesystem_file_data.cpp
src/filesystem_file_delete.cpp
src/filesystem_file_read.cpp
src/filesystem_command.cpp
src/filesystem_editor_adapter.cpp
```

主要职责：

- 实现 `touch` 创建空文件。
- 实现 `rm` 删除普通文件。
- 实现 `cat` 读取文件内容。
- 实现文件内容写入和 inode 文件大小更新。
- 维护 `nano` 命令和 ncurses 编辑器之间的适配逻辑。
- 维护 shell 提示符、命令解析、命令分发和 `help` 输出。

对外协作接口：

- 调用成员 A 的 block/inode 分配与释放接口。
- 调用成员 B 的目录项定位和权限判断逻辑。
- 为成员 D 的用户模块提供 `/etc/user`、`/etc/passwd`、`/etc/group` 文件写入能力。

验收重点：

- `touch a` 后 `ls` 能显示文件 `a`。
- `cat a` 能读取文件内容或保持空文件输出行为。
- `rm a` 后 `ls` 不再显示文件 `a`。
- `help` 能列出所有支持命令。
- 未知命令仍输出 `command not found: <command>`。
- `nano` 能进入编辑界面，保存后内容可被 `cat` 读取。

### 成员 D：用户、登录与测试集成模块

成员 D 负责多用户系统、登录校验、用户增删、测试脚本和项目集成文档。该部分保证系统能以多用户方式运行，并负责最终集成验证。

负责文件：

```text
src/filesystem_session.cpp
src/filesystem_user_add.cpp
src/filesystem_user_delete.cpp
src/filesystem_user_db.cpp
scripts/smoke_cli.py
Windows运行环境安装指南.md
协作开发分工文档.md
```

主要职责：

- 实现 `Login()` 登录流程。
- 实现用户名和密码校验。
- 维护当前登录用户、用户组、当前目录等会话状态。
- 实现 `useradd` 创建用户、用户主目录和 `/etc` 配置项。
- 实现 `userdel` 删除用户、删除用户主目录并更新 `/etc` 配置项。
- 维护 `scripts/smoke_cli.py` 冒烟测试脚本。
- 维护运行环境文档和协作开发文档。

对外协作接口：

- 调用成员 B 的目录创建、删除和路径跳转能力。
- 调用成员 C 的文件写入能力维护 `/etc/user`、`/etc/passwd` 和 `/etc/group`。
- 为所有成员提供统一测试脚本，保证模块修改后行为没有回归。

验收重点：

- 初始用户 `root/root` 能登录。
- `useradd bob` 后能使用 `bob` 登录。
- 新用户登录后默认进入 `/home/bob`。
- `userdel bob` 后用户信息从 `/etc` 相关文件中删除。
- 冒烟测试脚本能够在临时目录运行，不污染项目根目录的 `data.img`。

## 3. 模块之间的协作关系

四个成员的模块依赖关系可以概括为：

```text
成员 A：虚拟磁盘与空间管理
  ↓ 提供 inode/block/磁盘读写基础
成员 B：目录、路径与权限
  ↓ 提供目录定位、权限判断、目录操作
成员 C：文件、编辑器与命令交互
  ↓ 提供文件内容操作和 shell 入口
成员 D：用户、登录与测试集成
  ↔ 依赖目录和文件模块维护用户数据，同时负责整体测试
```

其中 `FileSystem` 是核心协调类，`main.cpp` 不直接调用四个成员的内部模块，而是统一通过 `FileSystem` 的公共接口完成初始化、登录、显示提示符和命令分发。

公共接口保持为：

```cpp
FileSystem(Image& _image, SuperBlock* _superBlock);
bool InitFileSystem();
bool Format();
bool MakeMenu();
bool Login();
void ReadCommand(char string[]);
bool is_login;
```

## 4. 协作开发流程

### 4.1 开发前约定

每位成员在修改代码前应先确认自己负责的模块，不跨模块随意修改他人代码。如果必须修改其他成员负责的文件，需要说明原因并同步相关成员。

每次修改前应先构建一次当前工程：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --build --preset msys2-ucrt64-debug
```

### 4.2 开发中约定

开发时应尽量保持以下原则：

- 一个源文件只承担一种主要职责。
- 不在用户模块中直接写复杂的磁盘块分配逻辑。
- 不在命令解析模块中直接实现底层文件读写细节。
- 不在目录模块中处理用户密码校验。
- 不在分配模块中输出用户命令层面的提示信息。
- 不为了方便测试而改变真实命令行为。

### 4.3 集成前检查

每个成员完成自己的模块修改后，都应至少执行：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --build --preset msys2-ucrt64-debug
python scripts\smoke_cli.py --exe build\FlyflyUFS.exe
```

冒烟测试通过后，才能认为本次修改可以进入集成。

## 5. 测试分工

测试工作由成员 D 统一组织，但每个成员都需要负责自己模块的基础测试。

| 测试内容 | 主要负责人 | 相关命令 |
|---|---|---|
| 文件系统格式化与加载 | 成员 A | 首次启动、二次启动 |
| inode/block 分配回收 | 成员 A | `touch`、`rm`、`mkdir`、`rmdir` |
| 目录创建、跳转、删除 | 成员 B | `mkdir`、`cd`、`pwd`、`rmdir` |
| 权限修改与目录显示 | 成员 B | `chmod`、`ls` |
| 文件创建、读取、删除 | 成员 C | `touch`、`cat`、`rm` |
| 命令解析与帮助输出 | 成员 C | `help`、未知命令 |
| 登录和用户管理 | 成员 D | `Login`、`useradd`、`userdel` |
| 总体验收脚本 | 成员 D | `scripts/smoke_cli.py` |

## 6. 推荐开发顺序

为了降低集成风险，四人协作时建议按以下顺序推进：

1. 成员 A 先完成虚拟磁盘、格式化、分配回收模块。
2. 成员 B 基于稳定的 inode/block 接口完成目录和权限模块。
3. 成员 C 基于目录模块完成普通文件操作和命令分发。
4. 成员 D 基于目录和文件模块完成用户系统与集成测试。
5. 四人共同进行最终联调，重点检查 `root/root` 登录、文件目录操作、用户增删和持久化。

## 7. 风险与注意事项

项目中最需要避免的问题是“一个成员为了实现自己的功能，直接修改其他模块的底层逻辑”，这样容易造成连锁错误。典型风险包括：

- 修改 `iNode`、`Dir`、`SuperBlock` 导致旧 `data.img` 无法读取。
- 修改 block/inode 分配逻辑导致文件或目录损坏。
- 修改命令格式导致测试脚本和演示流程失效。
- 修改登录逻辑导致 `root/root` 无法登录。
- 修改路径逻辑导致用户主目录跳转失败。
- 修改文件写入逻辑导致 `/etc/passwd`、`/etc/user` 或普通文件内容损坏。

因此，任何涉及磁盘布局、路径跳转、权限判断和用户数据库的修改，都必须通过完整冒烟测试后才能合并。

## 8. 最终验收标准

四人协作开发完成后，项目应满足以下条件：

- 可以通过 CMake 正常构建生成 `FlyflyUFS.exe`。
- 首次运行能够自动创建并格式化 `data.img`。
- 默认用户 `root/root` 能正常登录。
- 支持多级目录创建、进入、退出、删除和列表显示。
- 支持文件创建、读取、写入、删除。
- 支持用户添加、用户删除和新用户登录。
- 支持命令行交互和帮助信息输出。
- 支持通过 `scripts/smoke_cli.py` 进行自动化冒烟测试。
- 代码模块划分清晰，每个成员负责的模块边界明确。
