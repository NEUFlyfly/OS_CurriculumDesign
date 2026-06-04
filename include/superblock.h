//
// Created by 林智超 on 2019-06-16.
//

#ifndef UNIXFILESYSTEM_SUPERBLOCK_H
#define UNIXFILESYSTEM_SUPERBLOCK_H

#include "const.h"
#include <cstring>
using namespace std;


//对应blocksize
struct SuperBlock
{
    unsigned short s_INODE_NUM;				//inode节点数，最多 65535
    unsigned int s_BLOCK_NUM;				//磁盘块块数，最多 4294967294
    unsigned short s_free_INODE_NUM;		//空闲inode节点数
    unsigned int s_free_BLOCK_NUM;			//空闲磁盘块数
    int s_free_addr;						//空闲块堆栈指针
    int s_free[BLOCKS_PER_GROUP];			//空闲块堆栈
    unsigned short s_BLOCK_SIZE;			//磁盘块大小
    unsigned short s_INODE_SIZE;			//inode大小
    unsigned short s_SUPERBLOCK_SIZE;		//超级块大小
    unsigned short s_blocks_per_group;		//每 blockgroup 的block数量
    int s_Superblock_StartAddr;
    int s_InodeBitmap_StartAddr;
    int s_BlockBitmap_StartAddr;
    int s_Inode_StartAddr;
    int s_Block_StartAddr;
};




#endif //UNIXFILESYSTEM_SUPERBLOCK_H
