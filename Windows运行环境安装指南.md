# FlyflyUFS Windows 运行环境安装指南

本文档说明如何在 Windows 上安装 FlyflyUFS 所需的编译器、构建工具和运行库，并通过 VS Code 或命令行构建运行项目。

## 1. 为什么需要安装这些工具

FlyflyUFS 是一个 C++17 项目。项目代码本身是普通的 `.cpp` 和 `.h` 文件，但它依赖 `ncurses` 风格的终端库，并且需要 CMake/Ninja 来完成自动构建。

Windows 默认不自带这些工具：

- `g++`：C++ 编译器，负责把 `.cpp` 编译成程序。
- `cmake`：读取 `CMakeLists.txt`，生成构建规则。
- `ninja`：根据 CMake 生成的规则执行实际编译。
- `ncursesw`：提供终端界面相关能力。
- `gdb`：VS Code 调试 C++ 程序时使用的调试器。

本项目推荐使用 MSYS2 UCRT64 环境安装这些工具。

## 2. 安装 MSYS2

如果电脑还没有安装 MSYS2，可以使用 Windows 命令行安装。

打开 PowerShell 或 Windows Terminal，执行：

```powershell
winget install --id MSYS2.MSYS2 -e --source winget --accept-package-agreements --accept-source-agreements
```

安装完成后，默认路径通常是：

```text
C:\msys64
```

后续项目配置默认使用这个路径。如果你安装到了其他位置，需要同步修改项目里的：

```text
CMakePresets.json
.vscode/tasks.json
.vscode/launch.json
CMakeLists.txt
```

## 3. 初始化并更新 MSYS2

安装完成后，先更新 MSYS2 包管理器和基础系统。

在 PowerShell 中执行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
```

如果执行过程中提示需要关闭窗口或重启 MSYS2，这是正常的。然后再执行一次：

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
```

## 4. 安装项目所需包

继续在 PowerShell 中执行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-ncurses mingw-w64-ucrt-x86_64-gdb"
```

这些包分别提供：

| 包名 | 作用 |
|---|---|
| `mingw-w64-ucrt-x86_64-gcc` | 提供 `gcc` 和 `g++` 编译器 |
| `mingw-w64-ucrt-x86_64-cmake` | 提供 `cmake` |
| `mingw-w64-ucrt-x86_64-ninja` | 提供 `ninja` 构建工具 |
| `mingw-w64-ucrt-x86_64-ncurses` | 提供 `ncursesw` 终端库 |
| `mingw-w64-ucrt-x86_64-gdb` | 提供 VS Code 调试用的 `gdb` |

## 5. 检查安装是否成功

执行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:$PATH; g++ --version; cmake --version; ninja --version; gdb --version; pacman -Q mingw-w64-ucrt-x86_64-ncurses"
```

如果能看到 `g++`、`cmake`、`ninja`、`gdb` 的版本号，以及 `mingw-w64-ucrt-x86_64-ncurses` 的包信息，就说明环境安装成功。

## 6. 用命令行构建项目

进入项目目录：

```powershell
cd D:\code\OS_CurriculumDesign
```

配置项目：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --preset msys2-ucrt64-debug
```

编译项目：

```powershell
C:\msys64\ucrt64\bin\cmake.exe --build --preset msys2-ucrt64-debug
```

编译成功后，会生成：

```text
build\FlyflyUFS.exe
```

运行程序：

```powershell
.\build\FlyflyUFS.exe
```

默认登录账号：

```text
username: root
password: root
```

## 7. 用 VS Code 一键运行

项目已经包含 VS Code 配置：

```text
.vscode/tasks.json
.vscode/launch.json
.vscode/settings.json
```

推荐安装 VS Code 扩展：

- C/C++
- CMake Tools

然后在 VS Code 中打开项目根目录：

```text
D:\code\OS_CurriculumDesign
```

常用操作：

| 操作 | 效果 |
|---|---|
| `Ctrl + Shift + B` | 执行默认构建任务，生成 `build/FlyflyUFS.exe` |
| `F5` | 先构建，再用 GDB 启动调试 |
| Terminal -> Run Task -> Run FlyflyUFS | 构建并运行程序 |

## 8. 生成文件说明

构建后，项目根目录会出现：

```text
build/
```

这是 CMake/Ninja 生成的构建目录，里面包含：

```text
build/CMakeCache.txt
build/CMakeFiles/
build/build.ninja
build/FlyflyUFS.exe
```

这些文件都可以重新生成，不属于源码。

程序运行后，还可能出现：

```text
data.img
```

这是 FlyflyUFS 的虚拟磁盘文件，里面保存模拟文件系统的数据。它不是 CMake 生成物。

如果登录异常，或者想重置模拟文件系统，可以删除：

```text
data.img
```

然后重新运行程序，系统会重新初始化，默认账号会恢复为：

```text
root / root
```

注意：删除 `data.img` 会清空模拟文件系统里的所有数据。

## 9. 常见问题

### 9.1 提示找不到 `ncurses.h`

说明 `ncurses` 没有安装，或者 MSYS2 UCRT64 路径不对。

先确认安装：

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -Q mingw-w64-ucrt-x86_64-ncurses"
```

再确认文件存在：

```text
C:\msys64\ucrt64\include\ncursesw\ncurses.h
```

### 9.2 提示找不到 DLL

通常是运行程序时没有把 MSYS2 UCRT64 的 `bin` 目录加入 `PATH`。

需要确保环境变量中有：

```text
C:\msys64\ucrt64\bin
```

项目的 VS Code 配置已经自动设置了这个路径。

### 9.3 编译时报 `cannot open output file FlyflyUFS.exe: Permission denied`

通常是 `FlyflyUFS.exe` 正在运行，Windows 不允许覆盖正在运行的 exe。

解决方法：

1. 关闭正在运行的程序窗口。
2. 如果仍然失败，在任务管理器里结束 `FlyflyUFS.exe`。
3. 重新构建。

### 9.4 中文输出乱码

项目已经在程序启动时把 Windows 控制台切到 UTF-8。如果仍然乱码，可以先在终端执行：

```powershell
chcp 65001
```

然后重新运行程序。

### 9.5 `root/root` 登录失败

如果程序没有重新格式化，而是直接使用旧的 `data.img`，可能是虚拟磁盘里的数据已经损坏或不是默认状态。

如果不需要保留其中的数据，可以删除：

```text
data.img
```

然后重新运行程序。

## 10. 推荐目录状态

源码和配置主要保留：

```text
include/
src/
main.cpp
CMakeLists.txt
CMakePresets.json
.vscode/
README.md
项目介绍.md
Windows运行环境安装指南.md
```

可重新生成或运行时生成：

```text
build/
data.img
```

`.gitignore` 已经忽略 `build/` 和 `data.img`，所以它们不会污染 git 状态。
