//
// Created by 林智超 on 2019-06-16.
//

#ifndef UNIXFILESYSTEM_CONST_H
#define UNIXFILESYSTEM_CONST_H

// 文件类型标记，会和权限位一起存放在 iNode::inode_mode 中。
#define MODE_DIR	01000
#define MODE_FILE	00000

// 新建普通文件和目录时使用的默认 Unix 风格权限。
#define FILE_DEF_PERMISSION 0664
#define DIR_DEFAULT_PERMISSION	0755

// 命令处理和文件编辑过程中使用的临时缓冲区大小。
#define FILE_BUFFER 10000

// 虚拟磁盘布局参数。项目的所有文件系统数据都存放在 data.img 中，
// 因此这些值决定磁盘镜像的二进制结构，不应随意修改。
#define BLOCK_SIZE	512
#define INODE_SIZE	128

// 目录项中允许保存的最大文件名或目录名长度。
#define MAX_NAME_SIZE 28

// 模拟文件系统中的 inode 总数和数据块总数。
#define INODE_NUM	640
#define BLOCK_NUM	10240

// 超级块空闲块栈一次缓存的空闲块地址数量。
#define BLOCKS_PER_GROUP 64

// 虚拟磁盘镜像文件名。
#define IMAGEFILENAME "data.img"

// data.img 中各个区域的起始字节偏移。
// 整体布局为：超级块 -> inode 位图 -> block 位图 -> inode 区 -> 数据块区。
#define SUPERBLOCK_START_ADDR 0
// 超级块占用第 0 个 block，因此 inode 位图从第 1 个 block 开始：1 * 512 = 512。
#define INODEBITMAP_START_ADDR 1*BLOCK_SIZE
// 保留原有宏名拼写，因为源码中已经依赖这个名字。
// inode 位图占用 2 个 block，因此 block 位图起始地址为：512 + 2 * 512 = 1536。
#define BLOCKBIITMAP_START_ADDR INODEBITMAP_START_ADDR + 2*BLOCK_SIZE
// block 位图占用 20 个 block，因此 inode 区起始地址为：1536 + 20 * 512 = 11776。
#define INODE_START_ADDR BLOCKBIITMAP_START_ADDR + 20*BLOCK_SIZE
// 每个 block 大小为 512 字节，每个 inode 大小为 128 字节，所以 1 个 block 可存 4 个 inode。
// 系统一共有 640 个 inode，需要 640 / 4 = 160 个 block 存放 inode 区。
// 数据块区起始地址为：11776 + 160 * 512 = 93696。
#define BLOCK_STARTADDR INODE_START_ADDR + INODE_NUM/(BLOCK_SIZE/INODE_SIZE) * BLOCK_SIZE

// 根目录使用 inode 区中的第一个 inode，因此根目录 inode 地址就是 INODE_START_ADDR。
#define ROOT_DIR_ADDR INODE_START_ADDR

// 类 shell 提示符中显示的主机名。
#define HOSTNAME "chaosmyth"


#endif //UNIXFILESYSTEM_CONST_H
