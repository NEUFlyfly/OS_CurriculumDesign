# FlyflyUFS 工程说明与文件系统原理详解

本文面向需要理解本课程设计工程的读者，目标不是只列出命令，而是把“工程实现了哪些功能”“每个功能背后的文件系统原理是什么”“源码中怎样落地这些原理”连成一条完整线索。

工程名称为 **FlyflyUFS**。它是一个用 C++ 实现的教学型类 Unix 文件系统模拟器：运行时把普通宿主机文件 `data.img` 当作虚拟磁盘，在其中组织超级块、inode 位图、数据块位图、inode 表和数据块区域，并在此基础上实现目录、普通文件、权限、用户登录、用户管理、命令行交互和 ncurses 文本编辑器。仓库中还包含一个独立的浏览器 Web UI 模拟版本，用 IndexedDB 模拟类似的数据结构并提供可视化界面。

> 重要定位：这是一个操作系统课程设计/教学模拟项目，不是可直接用于生产环境的真实文件系统。文档中提到的“磁盘”“块”“inode”等概念，都指 `data.img` 或 Web UI 的模拟存储结构。

## 1. 工程整体概览

### 1.1 工程解决的问题

真实操作系统中的文件系统需要解决几个基本问题：

1. 如何把磁盘划分成可管理的区域。
2. 如何记录文件系统的整体状态，例如总 inode 数、空闲块数、空闲 inode 数。
3. 如何表示一个文件或目录的元数据，例如类型、权限、属主、大小、数据块位置。
4. 如何表示目录项，即“名字到 inode 的映射”。
5. 如何分配和回收 inode 与数据块。
6. 如何根据用户命令完成 `mkdir`、`touch`、`cat`、`rm`、`chmod` 等文件系统操作。
7. 如何保存用户、密码、组信息，并通过权限位控制访问。

FlyflyUFS 对这些问题都给出了一个较小但完整的模拟实现。它不是挂载到操作系统内核中的文件系统，而是一个用户态程序；所有数据都通过 C 标准 I/O 读写到 `data.img` 这个普通文件中。

### 1.2 已实现的主要功能

从用户可见角度看，C++ 命令行版本实现了这些能力：

| 功能类别 | 已实现内容 |
|---|---|
| 启动与格式化 | 首次运行自动创建 `data.img` 并格式化；root 用户可执行 `format` 重新格式化 |
| 登录/会话 | 用户名密码登录；默认用户 `root/root`；登录后进入用户家目录 |
| 目录操作 | `pwd`、`ls`、`cd`、`mkdir`、`rmdir` |
| 文件操作 | `touch`、`cat`、`rm`、`nano` |
| 权限管理 | 类 Unix 的 9 位 `rwx` 权限；`chmod` 修改权限；root 绕过大部分权限检查 |
| 用户管理 | `useradd`、`userdel`；系统文件 `/etc/user`、`/etc/passwd`、`/etc/group` 保存用户信息 |
| 交互体验 | 类 shell 提示符、`help` 帮助、`clear` 清屏、`exit` 退出 |
| 测试辅助 | `scripts/smoke_cli.py` 提供自动化冒烟测试 |
| 可视化辅助 | `webui/index.html` 提供独立浏览器模拟和磁盘状态可视化 |

从文件系统原理角度看，它实现或模拟了这些内容：

| 原理 | 工程实现 |
|---|---|
| 虚拟磁盘 | 使用 `data.img` 作为二进制磁盘镜像 |
| 超级块 | `SuperBlock` 保存全局容量、空闲数量、布局起始地址、空闲块栈 |
| inode | `iNode` 保存类型、权限、链接计数/目录项计数、属主、大小、直接块地址 |
| inode 位图 | `inode_bitmap[640]` 标记 inode 是否已占用 |
| 数据块位图 | `block_bitmap[10240]` 标记数据块是否已占用 |
| inode 表 | 固定 640 个 inode，每个 128 字节 |
| 数据块 | 每块 512 字节，用于保存目录项、文件内容或空闲块栈 |
| 目录项 | `Dir` 结构保存名称和 inode 地址，形成“文件名 -> inode”映射 |
| 空闲块管理 | 使用成组链接/空闲块栈思想，每组 64 个块地址 |
| 权限模型 | 使用文件类型位加 9 位权限位，按 owner/group/other 判断 |

### 1.3 工程中的两个实现版本

仓库中实际存在两个相关但独立的实现：

1. **C++ 命令行版本**：核心课程设计实现，入口为 `main.cpp`，运行结果保存在 `data.img`。
2. **Web UI 版本**：`webui/index.html` 中的浏览器模拟器，使用 JavaScript 和 IndexedDB 保存数据，用于演示、可视化和辅助理解。

两者都围绕 FlyflyUFS 的文件系统思想设计，但不是同一个持久化层：C++ 版本写 `data.img`，Web UI 写浏览器 IndexedDB。因此，Web UI 不能直接读取 C++ 生成的 `data.img`。

## 2. 仓库结构说明

核心文件和目录如下：

```text
OS_CurriculumDesign/
├── main.cpp                         程序入口，负责启动、格式化判断、登录和命令循环
├── CMakeLists.txt                   CMake 构建规则
├── CMakePresets.json                MSYS2 UCRT64 + Ninja 构建预设
├── include/
│   ├── const.h                      全局常量、磁盘布局、块大小、inode 数等
│   ├── datastruct.h                 SuperBlock、iNode、Dir 等核心数据结构
│   ├── filesystem.h                 FileSystem 类接口与状态封装
│   ├── image.h                      data.img 镜像文件封装
│   └── editor.h                     ncurses 文本编辑器结构与接口
├── src/
│   ├── image.cpp                    虚拟磁盘文件打开/创建逻辑
│   ├── filesystem.cpp               文件系统初始化、格式化、分配、回收等核心逻辑
│   ├── filesystem_command.cpp       命令解析与 help/pwd/exit/format 等调度逻辑
│   ├── filesystem_dir.cpp           目录查找、创建、删除、列表、权限修改
│   ├── filesystem_file.cpp          普通文件创建、读取、写入、删除
│   ├── filesystem_login.cpp         登录、退出、密码校验
│   ├── filesystem_user.cpp          useradd/userdel 与用户数据库更新
│   ├── filesystem_editor_adapter.cpp FileSystem 与 ncurses 编辑器适配
│   └── editor.cpp                   ncurses 页面编辑器实现
├── scripts/
│   └── smoke_cli.py                 自动化命令行冒烟测试
├── webui/
│   └── index.html                   独立浏览器模拟器和可视化界面
├── test_commands.txt                手工测试命令示例
├── windows_env_guide.md             Windows/MSYS2 环境搭建说明
├── dev_task_split.md                开发任务拆分说明
└── docs/
    └── FlyflyUFS_工程说明与文件系统原理详解.md 本文档
```

运行过程中还可能产生这些文件：

| 文件 | 作用 | 是否应提交 |
|---|---|---|
| `data.img` | C++ 版本的虚拟磁盘镜像 | 否，运行产物 |
| `bfile.txt` | 编辑器适配层使用的临时/辅助文件 | 否，运行产物 |
| `afile.txt` | 编辑器调试输出相关文件 | 否，运行产物 |
| `build/` | CMake 构建目录 | 否，构建产物 |

`.gitignore` 已经忽略了常见构建产物、`data.img`、`bfile.txt`、IDE 文件和 `.omo/` 等目录。

## 3. 构建与运行

### 3.1 推荐环境

工程面向 Windows + MSYS2 UCRT64 环境。`CMakePresets.json` 中的预设假定工具链位于：

```text
C:/msys64/ucrt64/bin/gcc.exe
C:/msys64/ucrt64/bin/g++.exe
C:/msys64/ucrt64/bin/cmake.exe
```

项目依赖 ncursesw 相关库，`CMakeLists.txt` 中链接了：

```text
ncursesw
systre
tre
intl
iconv
pcre2-posix
```

因此，环境中需要安装 GCC、CMake、Ninja、ncurses 等组件。详细安装步骤可参考仓库中的 `windows_env_guide.md`。

### 3.2 构建命令

使用仓库内预设配置：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --preset msys2-ucrt64-debug
C:\msys64\ucrt64\bin\cmake.exe --build --preset msys2-ucrt64-debug
```

构建成功后，可执行文件位于：

```text
build\FlyflyUFS.exe
```

### 3.3 运行与默认登录

运行程序：

```powershell
build\FlyflyUFS.exe
```

首次运行时，如果当前工作目录下不存在 `data.img`，程序会创建该文件并进行格式化。默认登录账号为：

```text
username: root
password: root
```

登录后通常进入：

```text
/home/root
```

如果想重置整个模拟文件系统，可删除 `data.img` 后重新运行，或在 root 登录状态下执行 `format`。

## 4. 程序启动流程

C++ 命令行版本的入口是 `main.cpp`，整体流程可以概括为：

```text
main()
  ├─ 设置 Windows 控制台 UTF-8 编码
  ├─ 清屏
  ├─ 创建 Image 对象
  ├─ Image::InitImage(isFormat)
  │    ├─ 如果 data.img 不存在：创建文件，并设置 isFormat = true
  │    └─ 如果 data.img 已存在：打开文件，并设置 isFormat = false
  ├─ 创建 SuperBlock
  ├─ 创建 FileSystem
  ├─ 如果 isFormat 为 true：调用 FileSystem::Format()
  ├─ 调用 FileSystem::InitFileSystem()
  ├─ 循环 Login()，直到登录成功
  ├─ 登录后循环：
  │    ├─ MakeMenu() 输出提示符
  │    ├─ 读取一行命令
  │    └─ ReadCommand() 解析并执行命令
  └─ 关闭镜像文件句柄，输出“登出”
```

这个流程体现了文件系统模拟器的一般启动逻辑：先确保底层存储介质存在，再读取或初始化文件系统元数据，然后进入用户会话和命令循环。

## 5. 虚拟磁盘与总体布局

### 5.1 `data.img` 的角色

真实文件系统把数据写入磁盘扇区或块设备。FlyflyUFS 没有直接操作物理磁盘，而是把普通文件 `data.img` 当作一块连续的虚拟磁盘。

`src/image.cpp` 中的 `Image::InitImage()` 负责打开或创建该文件。实现上使用单个 `rb+` 文件句柄同时读写，避免 Windows 环境下读写双句柄可能带来的缓冲同步问题。

从文件系统角度看，`data.img` 的字节空间被分成几个区域：

```text
偏移 0
│
├─ block 0: 超级块 SuperBlock
│
├─ block 1-2: inode 位图
│
├─ block 3-22: 数据块位图
│
├─ block 23-182: inode 表
│
└─ block 183 起: 数据块区域
```

### 5.2 关键常量

这些常量定义在 `include/const.h` 中：

| 常量 | 值 | 含义 |
|---|---:|---|
| `BLOCK_SIZE` | 512 | 每个数据块大小，单位字节 |
| `INODE_SIZE` | 128 | 每个 inode 大小，单位字节 |
| `MAX_NAME_SIZE` | 28 | 目录项中文件名最大字节长度 |
| `INODE_NUM` | 640 | inode 总数 |
| `BLOCK_NUM` | 10240 | 数据块总数 |
| `BLOCKS_PER_GROUP` | 64 | 成组链接/空闲块栈每组块地址数量 |
| `IMAGEFILENAME` | `data.img` | 虚拟磁盘文件名 |
| `HOSTNAME` | `chaosmyth` | shell 提示符中的主机名 |

### 5.3 关键偏移地址

`include/const.h` 中定义了各区域起始地址：

| 区域 | 起始地址 | 说明 |
|---|---:|---|
| 超级块 | `0` | `SUPERBLOCK_START_ADDR` |
| inode 位图 | `512` | `INODEBITMAP_START_ADDR`，占 2 个块 |
| 数据块位图 | `1536` | `BLOCKBIITMAP_START_ADDR`，占 20 个块 |
| inode 表 | `11776` | `INODE_START_ADDR`，占 160 个块 |
| 数据块区 | `93696` | `BLOCK_STARTADDR` |
| 根目录 inode | `11776` | `ROOT_DIR_ADDR`，即 inode 表第一个 inode |

计算过程如下：

```text
block 0                    = 512 字节超级块区域
inode bitmap 2 blocks       = 2 * 512 = 1024 字节
block bitmap 20 blocks      = 20 * 512 = 10240 字节
inode table 640 * 128 bytes = 81920 字节 = 160 blocks

数据块区起始地址 = 512 + 1024 + 10240 + 81920 = 93696
```

注意：`BLOCK_NUM = 10240` 指的是数据块区中的数据块数量，不包括前面的超级块、位图和 inode 表元数据区域。

## 6. 核心数据结构

核心结构定义在 `include/datastruct.h` 和 `include/filesystem.h`。

### 6.1 SuperBlock：超级块

超级块是文件系统的总账本，保存整体布局和剩余资源信息。FlyflyUFS 的 `SuperBlock` 包含：

| 字段类别 | 代表字段 | 作用 |
|---|---|---|
| 容量信息 | `s_INODE_NUM`、`s_BLOCK_NUM` | inode 和数据块总数 |
| 空闲信息 | `s_free_INODE_NUM`、`s_free_BLOCK_NUM` | 剩余 inode 和数据块数量 |
| 块大小信息 | `s_BLOCK_SIZE`、`s_INODE_SIZE` | 块和 inode 的固定大小 |
| 布局信息 | `s_superblock_start_addr`、`s_inodebitmap_start_addr`、`s_blockbitmap_start_addr`、`s_inode_start_addr`、`s_block_start_addr` | 各区域在 `data.img` 中的起始地址 |
| 空闲块栈 | `s_free_addr`、`s_free[64]` | 成组链接算法使用的当前空闲块地址栈 |

超级块保存在 `data.img` 的第 0 块。每次分配或释放 inode/数据块时，相关计数和空闲块栈都会写回超级块。

### 6.2 iNode：文件和目录的元数据

inode 用来描述一个文件或目录的元数据。FlyflyUFS 的 `iNode` 结构包含：

| 字段 | 作用 |
|---|---|
| `inode_id` | inode 编号 |
| `inode_mode` | 文件类型位和权限位 |
| `inode_cnt` | 对目录而言接近目录项/链接计数；对文件而言用于展示 |
| `user_name[20]` | 属主用户名 |
| `user_group[20]` | 所属用户组 |
| `inode_size` | 文件大小或目录占用大小 |
| `inode_dirblock[10]` | 最多 10 个直接数据块地址 |
| `inode_indirect_block_first` | 一级间接块字段，当前 C++ 实现中没有真正使用 |

一个 inode 不直接保存文件名。文件名保存在父目录的数据块里，通过目录项映射到 inode 地址。这是 Unix 类文件系统的核心思想：**文件名属于目录，文件元数据属于 inode**。

### 6.3 Dir：目录项

目录本质上是一种特殊文件，它的数据内容是一组目录项。FlyflyUFS 的目录项结构为：

```cpp
struct Dir {
    char name[28];
    int inode_addr;
};
```

一个目录项占 32 字节，一个数据块 512 字节，因此一个目录数据块可保存：

```text
512 / 32 = 16 个目录项
```

目录项把名字映射到 inode 地址。例如父目录 `/home` 中可以有：

```text
name = root, inode_addr = /home/root 对应 inode 的字节地址
```

### 6.4 FileSystemStorage 与 FileSystemState

`include/filesystem.h` 将文件系统内部状态拆成两类：

| 结构 | 主要内容 | 含义 |
|---|---|---|
| `FileSystemStorage` | `Image&`、`SuperBlock*`、`inode_bitmap[640]`、`block_bitmap[10240]` | 与磁盘和元数据缓存有关 |
| `FileSystemState` | 当前目录地址、当前用户、当前组、当前路径、家目录、UID/GID 计数 | 与当前登录会话有关 |

这种拆分让代码逻辑上区分“持久化存储状态”和“当前用户会话状态”。

## 7. 格式化过程详解

格式化由 `FileSystem::Format()` 完成。格式化会重建整个文件系统，因此会清空原有数据。

### 7.1 格式化的主要步骤

整体流程如下：

```text
Format()
  ├─ 初始化超级块字段
  ├─ 清空 inode 位图
  ├─ 清空数据块位图
  ├─ 初始化数据块区的空闲块成组链接结构
  ├─ 写回超级块
  ├─ 分配根目录 inode
  ├─ 分配根目录数据块
  ├─ 写入根目录目录项 .
  ├─ 写入根目录 inode
  ├─ 创建 /home
  ├─ 创建 /home/root
  ├─ 创建 /etc
  ├─ 创建 /etc/user
  ├─ 创建 /etc/passwd
  └─ 创建 /etc/group
```

格式化结束后，文件系统中至少存在这些目录和文件：

```text
/
├── home/
│   └── root/
└── etc/
    ├── user
    ├── passwd
    └── group
```

### 7.2 根目录初始化

根目录使用 inode 表中的第一个 inode，其地址是 `ROOT_DIR_ADDR`，也就是 `INODE_START_ADDR`。

根目录 inode 的关键属性：

| 属性 | 值/含义 |
|---|---|
| inode 地址 | `11776` |
| 类型 | 目录 |
| 权限 | `0755` |
| 属主 | `root` |
| 属组 | `root` |
| 第一个数据块 | 由 `BlockAlloc()` 分配 |
| 初始目录项 | `.` |

根目录只写入 `.`，而普通新建目录会写入 `.` 和 `..`。

### 7.3 系统用户文件

格式化期间会创建三个系统文件：

| 文件 | 初始内容 | 作用 |
|---|---|---|
| `/etc/user` | `root:x:0:0\n` | 保存用户、UID、GID 信息 |
| `/etc/passwd` | `root:root\n` | 保存用户名和明文密码 |
| `/etc/group` | `root::0:root\nuser::1:\n` | 保存用户组和组成员 |

这些文件借鉴了 Unix/Linux 的 `/etc/passwd`、`/etc/group` 思路，但实现非常简化，而且密码是明文保存。这是教学模拟，不具备真实系统的安全性。

## 8. inode 与数据块分配原理

### 8.1 inode 分配

inode 分配由 `INodeAlloc()` 完成。逻辑为：

1. 检查超级块中的空闲 inode 数是否为 0。
2. 从 `inode_bitmap` 中线性扫描第一个未使用位置。
3. 将超级块空闲 inode 数减 1。
4. 写回超级块。
5. 将位图中对应位置置为已占用并写回 `data.img`。
6. 返回 inode 的字节地址。

inode 地址计算公式：

```text
inode_addr = INODE_START_ADDR + inode_index * INODE_SIZE
```

例如第 0 个 inode 地址为：

```text
11776 + 0 * 128 = 11776
```

第 1 个 inode 地址为：

```text
11776 + 1 * 128 = 11904
```

### 8.2 inode 回收

inode 回收由 `INodeFree(addr)` 完成。它会：

1. 检查地址是否落在 inode 表范围内。
2. 检查地址是否按 `INODE_SIZE` 对齐。
3. 检查位图中该 inode 是否确实已占用。
4. 将 inode 区域清零。
5. 超级块空闲 inode 数加 1。
6. 清除 inode 位图。

这对应真实文件系统中的 inode 释放过程：只有确认元数据对象有效且已分配，才能释放并重新进入空闲池。

### 8.3 数据块分配：成组链接思想

数据块分配由 `BlockAlloc()` 完成。FlyflyUFS 同时维护了数据块位图和超级块中的空闲块栈。

超级块中有：

```text
s_free_addr        当前成组链接块地址
s_free[64]         当前组的空闲块地址栈
s_free_BLOCK_NUM   剩余空闲数据块数
```

分配时通过：

```text
top = (s_free_BLOCK_NUM - 1) % s_blocks_per_group
```

判断当前应从空闲块栈的哪个位置取块。

如果 `top != 0`，说明当前 `s_free[]` 中还有普通空闲块地址，直接取 `s_free[top]`。

如果 `top == 0`，说明当前组即将用完，需要取 `s_free_addr` 指向的块，并从该块读取下一组空闲块地址到 `s_free[]`。这就是成组链接的核心思想：**不是把所有空闲块地址都放在超级块中，而是让部分空闲块本身保存下一组空闲块地址**。

这种方式兼具两个优点：

1. 超级块中只需要保存有限数量的空闲块地址。
2. 分配大多数块时不用遍历整张位图。

### 8.4 数据块回收

数据块回收由 `FreeBlock(addr)` 完成。它会：

1. 检查数据块地址是否合法且对齐。
2. 检查数据块位图确认该块已被占用。
3. 清空该数据块内容。
4. 如果当前空闲块栈未满，把释放的块地址压入 `s_free[]`。
5. 如果当前空闲块栈已满，把释放的块本身作为新的成组链接块，保存旧的 `s_free_addr` 和当前栈内容。
6. 超级块空闲块数加 1。
7. 清除数据块位图。

这对应真实文件系统中的块回收：释放数据块时不仅要清除数据，也要更新空闲空间管理结构，否则会产生空间泄漏或重复分配。

## 9. 目录系统实现

### 9.1 目录为什么也是文件

在 Unix 类文件系统中，目录本质上也是一种文件。区别在于普通文件的数据块保存用户内容，而目录的数据块保存目录项。

FlyflyUFS 也采用这个模型：目录 inode 的 `inode_dirblock[]` 指向一个或多个数据块，每个数据块中按 32 字节一项保存 `Dir` 结构。

目录项示意：

```text
目录 inode
  └─ 数据块
      ├─ Dir{name=".",  inode_addr=当前目录 inode 地址}
      ├─ Dir{name="..", inode_addr=父目录 inode 地址}
      ├─ Dir{name="a",  inode_addr=文件 a 的 inode 地址}
      └─ Dir{name="d",  inode_addr=目录 d 的 inode 地址}
```

### 9.2 查找目录

`FindDir(inode_addr, name)` 用于从某个目录 inode 出发查找子目录。它会扫描该目录最多 10 个直接数据块，每个块最多 16 个目录项，因此结构上最多可扫描：

```text
10 * 16 = 160 个目录项
```

查找到名字后，还会读取目标 inode，确认目标是目录，并检查执行权限。目录的执行权限表示是否允许进入/穿过目录。

路径更新规则包括：

| 输入 | 路径变化 |
|---|---|
| `cd .` | 当前路径不变 |
| `cd ..` | 当前路径回到上一级 |
| `cd name` | 当前路径追加 `/name` |

C++ 命令行版本的 `cd` 是按单个目录名进行相对导航，不是完整路径解析器。也就是说，`cd d`、`cd ..`、`cd .` 是主要支持形式；不要把 C++ CLI 理解为完整支持 `cd /home/root/d` 这类绝对路径。

### 9.3 创建目录

`MakeDir(parent_addr, name)` 创建目录，关键步骤为：

1. 检查目录名长度是否小于 `MAX_NAME_SIZE`。
2. 读取父目录 inode。
3. 检查当前用户是否有父目录写权限，root 可绕过。
4. 扫描父目录已有目录项，拒绝重名。
5. 找到父目录中的空目录项位置。
6. 分配新 inode。
7. 分配新数据块。
8. 在新目录数据块中写入 `.` 和 `..`。
9. 写入新目录 inode。
10. 在父目录中插入新目录项。
11. 增加父目录的计数并写回。

新目录在 `MakeDir()` 内部按默认目录权限 `0755` 创建。但通过命令行执行 `mkdir` 时，`ReadCommand()` 随后又调用 `Chmod(..., 0660)`，因此实际命令创建出的目录在 `ls` 中会显示类似：

```text
drw-rw----
```

这是源码行为，文档和测试应以实际命令行为准。

### 9.4 删除目录

`rmdir name` 由 `DeleteDir()` 处理。它会：

1. 拒绝删除 `.` 或 `..`。
2. 检查父目录写权限。
3. 查找目标目录项。
4. 递归调用 `DeleteFileOrDir()` 删除目标目录下内容。
5. 清除父目录中的目录项。
6. 减少父目录计数并写回。

`DeleteFileOrDir()` 会根据 inode 类型判断目标是文件还是目录。目录中若还有子项，会继续递归删除。这比传统 Unix 的 `rmdir` 更接近“递归删除目录”的行为。

### 9.5 显示目录内容

`ls` 对应 `ShowDir()`。输出格式类似：

```text
drw-rw----  2  root  root  512  d
-rw-rw-r--  1  root  root  0    a
```

首字符表示类型：

| 字符 | 含义 |
|---|---|
| `d` | 目录 |
| `-` | 普通文件 |

后面 9 个字符表示 owner/group/other 的 `rwx` 权限。权限位来自 `inode_mode` 的低 9 位。

## 10. 普通文件实现

### 10.1 创建文件

文件创建有两个入口：

| 用户命令/内部函数 | 作用 |
|---|---|
| `touch name` -> `MakeFile()` | 创建空文件 |
| `Create(parent, name, content)` | 内部创建文件并写入初始内容 |

创建普通文件时会：

1. 检查文件名长度。
2. 检查父目录写权限。
3. 检查父目录下是否已有同名普通文件。
4. 找到父目录中的空目录项。
5. 分配 inode。
6. 根据内容长度分配数据块。
7. 将内容按 512 字节一块写入数据块。
8. 写入文件 inode。
9. 在父目录中插入目录项。

空文件也会分配至少一个数据块，这是当前实现的选择。

普通文件默认权限为 `0664`，因此 `ls` 中通常显示：

```text
-rw-rw-r--
```

### 10.2 读取文件

`cat name` 由 `Cat()` 实现。它会：

1. 在当前目录查找名为 `name` 的目录项。
2. 读取目标 inode。
3. 确认目标是普通文件。
4. 检查当前用户是否有读权限，root 可绕过。
5. 按 inode 中的数据块地址读取内容。
6. 根据 `inode_size` 控制输出字节数。

如果找不到普通文件，会输出类似：

```text
cat missing : No such file
```

### 10.3 写入文件

`WriteFile(inode, inode_addr, buffer)` 用于覆盖写入已有文件内容，例如 `nano` 保存文件、`useradd` 更新 `/etc/passwd` 等都会用到它。

它会按新内容长度把缓冲区分块写入直接块，并更新 `inode_size`。如果原来已有数据块且新内容仍需要对应块，会复用或写入这些块；如果需要更多块，会分配新块。

需要注意当前实现的一个限制：当新内容比旧内容短时，`WriteFile()` 并不会完整回收旧内容多出来的数据块，因此可能存在空间不能及时释放的问题。Web UI 版本的 JavaScript 实现中对此做了不同处理，会在写入前释放旧块。

### 10.4 删除文件

`rm name` 由 `DelFile()` 实现。它会：

1. 检查父目录写权限。
2. 查找目标普通文件。
3. 释放文件 inode 指向的数据块。
4. 释放文件 inode。
5. 清除父目录目录项。
6. 减少父目录计数并写回。

这对应真实文件系统删除文件时的主要动作：删除目录项只是第一步，还必须释放 inode 和数据块，否则存储空间不会回到空闲池。

## 11. 文件大小与目录容量限制

FlyflyUFS 的 inode 中有：

```text
inode_dirblock[10]
inode_indirect_block_first
```

但 C++ 当前实现只实际使用 10 个直接块，并没有真正实现一级间接块。因此普通文件最大可直接保存的数据大约为：

```text
10 * 512 = 5120 字节
```

虽然 `const.h` 中有 `FILE_BUFFER = 10000`，编辑器和缓冲区也可能允许更大的中间文本，但从 inode 数据块寻址能力看，C++ 文件持久化实现应按 10 个直接块理解。

目录结构理论上同样有 10 个直接块，每块 16 个目录项，结构上可扫描 160 个目录项。但新建目录时只分配一个数据块，`MakeDir()` 和 `Create()` 当前主要是在已有目录块中寻找空目录项，并没有在目录满时自动扩展新目录数据块。因此实际单目录容量应保守理解为：结构支持扫描 10 个直接块，但当前创建路径不完整实现目录自动扩容。

## 12. 权限系统

### 12.1 文件类型位和权限位

`inode_mode` 同时保存文件类型和权限。常量包括：

```text
MODE_DIR  = 01000
MODE_FILE = 00000
```

低 9 位保存类 Unix 权限：

```text
owner: rwx
group: rwx
other: rwx
```

例如：

```text
0755 = rwxr-xr-x
0664 = rw-rw-r--
0660 = rw-rw----
0600 = rw-------
```

### 12.2 权限检查规则

源码中的权限检查大致遵循：

1. 如果当前用户是 `root`，通常直接允许。
2. 如果当前用户名等于 inode 的 `user_name`，检查 owner 权限位。
3. 否则如果当前用户组等于 inode 的 `user_group`，检查 group 权限位。
4. 否则检查 other 权限位。

不同操作需要不同权限：

| 操作 | 主要权限要求 |
|---|---|
| `cd` 进入目录 | 目标目录执行权限 |
| `ls` 查看目录 | 当前目录读权限 |
| `mkdir`/`touch`/`rm`/`rmdir` | 父目录写权限 |
| `cat` | 文件读权限 |
| `nano` 读取已有文件 | 文件读权限 |
| `nano` 保存已有文件 | 文件写权限 |
| `chmod` | 文件/目录属主或 root |

源码中部分权限错误信息拼写为 `Permission Dennied`，这是当前程序实际输出的一部分。

### 12.3 chmod

命令格式为：

```text
chmod filename permissions
```

例如：

```text
chmod a 600
```

`Chmod()` 会保留 inode 中的文件类型位，只替换低 9 位权限：

```text
new_mode = old_type_bits | new_permission_bits
```

这样修改权限时不会把目录误改成普通文件，也不会把普通文件误改成目录。

## 13. 用户、登录与系统文件

### 13.1 登录流程

登录由 `Login()` 和 `check()` 实现。流程为：

```text
Login()
  ├─ 输入 username
  ├─ 输入 password
  ├─ check(username, password)
  │    ├─ 进入 /etc
  │    ├─ 读取 /etc/user
  │    ├─ 读取 /etc/passwd
  │    ├─ 检查用户是否存在
  │    ├─ 提取密码并比较
  │    ├─ 设置当前用户和用户组
  │    └─ 进入 /home/<username>
  └─ 设置 is_login
```

root 用户属于 `root` 组，其他普通用户属于 `user` 组。

### 13.2 用户数据库文件

FlyflyUFS 使用三个普通文件保存用户信息：

| 文件 | 示例 | 说明 |
|---|---|---|
| `/etc/user` | `root:x:0:0` | 用户名、占位字段、UID、GID |
| `/etc/passwd` | `root:root` | 用户名、密码 |
| `/etc/group` | `root::0:root` | 组名、占位字段、GID、组成员 |

这套设计借鉴 Linux，但没有哈希密码、salt、shadow 文件、严格解析器等安全机制。

### 13.3 添加用户

`useradd username` 由 `AddUser()` 实现，只有 root 可执行。它会：

1. 保存当前 root 会话状态。
2. 回到 `/home`。
3. 临时把当前用户名设为新用户，组设为 `user`，从而创建属主为新用户的家目录。
4. 创建 `/home/<username>`。
5. 将该目录权限改为 `0600`。
6. 进入 `/etc`。
7. 提示输入密码。
8. 更新 `/etc/user`、`/etc/passwd`、`/etc/group`。
9. 恢复 root 原来的会话状态。

添加成功后，新用户可用设置的密码登录，并进入自己的家目录。

### 13.4 删除用户

`userdel username` 由 `DeleteUser()` 实现，设计上只有 root 可执行。它会：

1. 检查目标用户。
2. 从 `/etc/user` 中删除用户行。
3. 从 `/etc/passwd` 中删除密码行。
4. 从 `/etc/group` 中删除组成员。
5. 删除 `/home/<username>`。

源码中存在一些需要注意的实现细节：

1. 删除 root 时会输出 `无法删除root 用户`，但源码中该分支后没有立即 `return`，因此文档和测试中不应过度假设 root 删除保护完全严密。
2. 删除用户内容使用子串查找，用户名包含关系可能导致不精确匹配。
3. 更新用户文件时有一处调用参数疑似把 user 文件内容写到了 passwd inode 地址相关位置，这属于当前实现的潜在缺陷。

这些问题不影响理解课程设计的总体结构，但在说明“限制与已知问题”时应明确写出。

## 14. 命令参考

C++ 命令行版本的命令由 `src/filesystem_command.cpp` 中的 `ReadCommand()` 分发。

| 命令 | 格式 | 功能 |
|---|---|---|
| `help` | `help` | 输出命令帮助 |
| `pwd` | `pwd` | 输出当前路径 |
| `ls` | `ls` | 列出当前目录内容 |
| `cd` | `cd dirname` | 进入子目录、`.` 或 `..` |
| `mkdir` | `mkdir dirname` | 创建目录 |
| `rmdir` | `rmdir dirname` | 删除目录，当前实现会递归删除内容 |
| `touch` | `touch filename` | 创建空普通文件 |
| `cat` | `cat filename` | 输出普通文件内容 |
| `rm` | `rm filename` | 删除普通文件 |
| `nano` | `nano filename` | 打开 ncurses 编辑器编辑文件 |
| `chmod` | `chmod filename permissions` | 修改权限，例如 `chmod a 600` |
| `useradd` | `useradd username` | root 添加用户 |
| `userdel` | `userdel username` | root 删除用户 |
| `format` | `format` | root 重新格式化文件系统并退出登录 |
| `clear` | `clear` | 清屏 |
| `exit` | `exit` | 退出当前会话 |

未知命令会输出：

```text
command not found: <command>
```

### 14.1 示例操作：文件生命周期

```text
root
root
pwd
ls
touch a
ls
rm a
exit
```

预期现象：

1. 登录后位于 `/home/root`。
2. `touch a` 创建普通空文件。
3. `ls` 可看到 `-rw-rw-r--` 权限的文件。
4. `rm a` 删除文件。
5. `exit` 后程序输出 `登出`。

### 14.2 示例操作：目录生命周期

```text
root
root
mkdir d
cd d
pwd
cd ..
rmdir d
exit
```

预期现象：

1. `mkdir d` 创建目录。
2. `cd d` 进入目录后 `pwd` 显示 `/home/root/d`。
3. 命令行创建的目录权限通常显示为 `drw-rw----`。
4. `rmdir d` 删除目录。

### 14.3 示例操作：添加用户并持久化

```text
root
root
useradd bob
bobpass
exit
```

重新运行程序后：

```text
bob
bobpass
pwd
exit
```

预期现象：

1. bob 用户可登录。
2. 登录后提示符中出现 `bob@chaosmyth`。
3. 当前路径为 `/home/bob`。

这个示例说明用户信息和家目录都持久化到了 `data.img` 中。

## 15. 文本编辑器 nano

`nano filename` 并不是调用系统自带 nano，而是使用工程中的 ncurses 编辑器实现。

相关文件：

| 文件 | 作用 |
|---|---|
| `include/editor.h` | 定义 `Line`、`Page`、按键处理相关接口 |
| `src/editor.cpp` | 实现页面缓冲区、插入、删除、移动、保存等逻辑 |
| `src/filesystem_editor_adapter.cpp` | 把编辑器缓冲区和 FlyflyUFS 文件读写连接起来 |

编辑器状态栏提示：

```text
F4:Exit F5:Save&Exit
```

主要按键：

| 按键 | 行为 |
|---|---|
| 方向键 | 移动光标 |
| 普通可打印字符 | 插入字符 |
| Enter | 插入新行 |
| Tab | 插入若干空格 |
| Backspace/Delete | 删除字符或合并行 |
| F4 | 不保存退出 |
| F5 | 保存并退出 |

编辑已有文件时，会先检查读权限；保存已有文件时，需要写权限。编辑不存在的文件时，保存时会调用 `Create()` 创建新文件，此时需要父目录写权限。

## 16. Web UI 版本说明

`webui/index.html` 是一个独立的浏览器版模拟器。它不是 C++ 程序的前端外壳，而是在浏览器中重新实现了一套类似 FlyflyUFS 的文件系统模型。

### 16.1 持久化方式

C++ 版本：

```text
data.img
```

Web UI 版本：

```text
IndexedDB 数据库 FlyflyUFS，version 2
object stores: meta, inodes, blocks, users
```

因此，两者数据不互通。

### 16.2 Web UI 支持的能力

Web UI 支持终端、侧边栏操作、模态框和磁盘可视化。命令包括：

```text
login, logout/exit, ls, pwd, cd, mkdir, rmdir, touch, cat, rm,
nano, chmod, useradd, userdel, help, status, infolist, clear, format
```

其中 `status` 会展示更强的可视化信息，例如：

1. 磁盘使用率卡片。
2. inode 使用率卡片。
3. 环形统计图。
4. 块位图。
5. 数据块详情。
6. 目录树。
7. 用户存储使用情况。
8. 超级块字段展示。

### 16.3 与 C++ 版本的主要差异

| 方面 | C++ 命令行版本 | Web UI 版本 |
|---|---|---|
| 持久化介质 | `data.img` | IndexedDB |
| 是否同一份数据 | 是 C++ 自己的数据 | 独立数据，不读取 `data.img` |
| 路径支持 | 主要是相对单级 `cd` | JavaScript 中支持更完整的绝对/相对路径解析 |
| 可视化 | 文本输出 | 仪表盘、块图、目录树 |
| 写文件收缩处理 | 可能不完整释放旧块 | JS 实现中有释放旧块逻辑 |

适合把 Web UI 当作“原理展示器”和“交互式演示版本”，而不是 C++ 文件系统的直接图形前端。

## 17. 测试与验证

### 17.1 自动化冒烟测试

仓库提供了 `scripts/smoke_cli.py`。该脚本会在临时目录中运行可执行文件，避免测试生成的 `data.img` 污染仓库根目录。

典型运行方式：

```powershell
python scripts\smoke_cli.py --exe build\FlyflyUFS.exe
```

覆盖场景包括：

| 场景 | 覆盖内容 |
|---|---|
| `root_file_lifecycle` | root 登录、`pwd`、`ls`、`touch`、`rm`、退出 |
| `directory_lifecycle` | `mkdir`、`cd`、`pwd`、`rmdir` |
| `command_errors` | `help`、未知命令、错误 `chmod`、缺失文件 `cat` 等 |
| `user_persistence` | `useradd` 后重启，再用新用户登录 |

这些测试说明项目至少验证了基本命令、错误提示、用户持久化和退出流程。

### 17.2 手工测试

`test_commands.txt` 中列出了手工测试命令，覆盖：

1. `help`、`pwd`、`ls`。
2. `mkdir`、`cd`、`rmdir`。
3. `touch`、`cat`、`rm`。
4. 多级目录和文件操作。
5. `useradd`、`userdel`。
6. root 权限。
7. `format`。
8. `exit`。

需要注意，其中部分示例看起来使用了绝对路径形式；而 C++ CLI 的源码主要按单级相对目录名查找。因此使用手工测试时，应以当前源码实际支持的命令形式为准。

## 18. 从操作系统原理角度理解本工程

### 18.1 文件系统层次

可以把 FlyflyUFS 理解为四层：

```text
用户命令层
  └─ ReadCommand(), MakeMenu(), Login(), Help()

文件/目录语义层
  └─ MakeDir(), DeleteDir(), MakeFile(), Cat(), DelFile(), Chmod()

元数据管理层
  └─ iNode, Dir, SuperBlock, inode_bitmap, block_bitmap

虚拟磁盘 I/O 层
  └─ Image, FILE*, fseek(), fread(), fwrite(), data.img
```

真实文件系统通常也有类似分层，只是底层不是 `FILE*`，而是块设备驱动、缓存层和内核 VFS。

### 18.2 一次 `touch a` 背后发生什么

用户输入：

```text
touch a
```

内部发生：

1. `ReadCommand()` 解析出 `touch` 和文件名 `a`。
2. 调用 `MakeFile(cur_dir_addr, "a")`。
3. 检查当前目录写权限。
4. 扫描当前目录数据块，确认没有同名普通文件。
5. 找到空目录项。
6. 调用 `INodeAlloc()` 分配 inode。
7. 调用 `Create()` 创建空文件。
8. `Create()` 至少分配一个数据块。
9. 写入文件 inode：类型为普通文件，权限为 `0664`，属主为当前用户。
10. 在父目录数据块中写入目录项 `a -> inode_addr`。
11. 更新父目录 inode 计数。

这正是文件创建的核心：**分配元数据、分配数据空间、建立名字到 inode 的映射**。

### 18.3 一次 `cat a` 背后发生什么

用户输入：

```text
cat a
```

内部发生：

1. 在当前目录扫描目录项，找到名字为 `a` 的条目。
2. 根据目录项中的 inode 地址读取 inode。
3. 确认 inode 类型是普通文件。
4. 检查读权限。
5. 根据 `inode_size` 和 `inode_dirblock[]` 读取数据块。
6. 输出文件内容。

这体现了文件读取路径：**文件名 -> 目录项 -> inode -> 数据块地址 -> 数据内容**。

### 18.4 一次 `rm a` 背后发生什么

用户输入：

```text
rm a
```

内部发生：

1. 检查当前目录写权限。
2. 找到 `a` 的目录项。
3. 读取 `a` 的 inode。
4. 根据 inode 中的数据块地址释放数据块。
5. 释放 inode。
6. 清空父目录中的 `a` 目录项。
7. 更新父目录计数。

这说明删除文件并不是“擦掉文件名”这么简单；真正重要的是把 inode 和数据块都归还给空闲空间管理系统。

### 18.5 一次 `mkdir d` 背后发生什么

用户输入：

```text
mkdir d
```

内部发生：

1. 检查当前目录写权限。
2. 检查目录名是否重名。
3. 分配目录 inode。
4. 分配目录数据块。
5. 在新目录数据块中写入 `.` 和 `..`。
6. 在父目录中写入 `d -> 新 inode 地址`。
7. 命令层把新目录权限改为 `0660`。

这里 `.` 和 `..` 是理解目录树的重要概念：`.` 指向自己，`..` 指向父目录。

## 19. 已知限制与注意事项

该项目是课程设计，代码中存在一些简化和限制。理解这些限制有助于避免把它误认为完整生产级文件系统。

| 限制/问题 | 说明 |
|---|---|
| 没有真正实现间接块 | `inode_indirect_block_first` 字段存在，但 C++ 文件读写主要使用 10 个直接块 |
| 文件大小有限 | 普通文件有效持久化能力应按约 5120 字节理解 |
| 目录扩容不完整 | 目录结构可扫描 10 个直接块，但创建逻辑没有完整实现目录满时自动分配新目录块 |
| C++ CLI 路径解析较简单 | 主要支持单级相对目录名，不是完整绝对路径解析 |
| 密码明文保存 | `/etc/passwd` 直接保存明文密码，只适合教学 |
| 用户名查找较粗糙 | `useradd`/`userdel` 使用子串查找，可能受用户名包含关系影响 |
| 删除 root 保护不够严密 | `DeleteUser()` 中打印无法删除 root 后没有立即返回 |
| 文件缩短时块回收不完整 | `WriteFile()` 可能不释放旧文件多余数据块 |
| 权限错误提示有拼写问题 | 源码中有 `Permission Dennied` |
| Web UI 与 C++ 数据不互通 | Web UI 使用 IndexedDB，不读取 `data.img` |
| `clear` 跨平台行为不完全一致 | 启动清屏使用 `cls`，命令清屏使用 `clear` |

这些限制不等于项目失败。相反，它们恰好可以作为课程答辩时讨论“真实文件系统还需要补什么”的切入点。

## 20. 可以如何继续完善

如果要把该项目继续向更完整的文件系统推进，可以考虑：

1. 实现一级间接块，使文件突破 10 个直接块限制。
2. 为目录实现自动扩容，新目录项不足时分配新的目录数据块。
3. 改进路径解析，支持 `/` 开头的绝对路径和多级相对路径。
4. 改进用户数据库解析，避免 `strstr` 子串匹配导致误判。
5. 删除用户时严格保护 root，并修复潜在的写错 inode 地址问题。
6. 文件覆盖写入前释放旧块，避免空间泄漏。
7. 增加一致性检查工具，例如检查位图、inode 表和目录项是否互相一致。
8. 为 `chmod`、`userdel`、目录递归删除、权限拒绝等场景增加自动化测试。
9. 将 Web UI 与 C++ 镜像格式打通，或者明确将 Web UI 定位为完全独立模拟器。
10. 改进密码保存方式，即使是教学项目也可以演示哈希思想。

## 21. 学习总结

FlyflyUFS 的价值在于把文件系统课本中的抽象概念落到了可以运行的代码里：

1. `data.img` 让读者看到“磁盘只是可按偏移读写的连续空间”。
2. 超级块让读者理解文件系统为什么需要全局元数据。
3. inode 位图和数据块位图展示了资源分配状态如何记录。
4. inode 表展示了“文件元数据”和“文件名”为什么要分开。
5. 目录项展示了目录树如何由名字到 inode 的映射构成。
6. 成组链接空闲块管理展示了比简单线性扫描更接近传统 Unix 的空间管理思想。
7. `mkdir`、`touch`、`cat`、`rm` 等命令把用户操作映射到 inode 和数据块的分配、查找、读取、释放。
8. 权限和用户系统展示了文件系统不仅管理数据，也管理访问控制。

如果用一句话概括本工程：**FlyflyUFS 是一个以 `data.img` 为虚拟磁盘、以超级块/inode/位图/目录项为核心结构、以类 Unix 命令为交互入口的教学型用户态文件系统模拟器。**
