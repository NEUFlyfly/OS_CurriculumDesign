//
// Created by 林智超 on 2019-06-16.
//

#ifndef UNIXFILESYSTEM_INODE_H
#define UNIXFILESYSTEM_INODE_H

#include "const.h"
#include <cstring>
#include <ctime>
using namespace std;
//size = 128

//混合索引
struct iNode {
    unsigned short inode_id;
    unsigned short inode_mode;
    unsigned short inode_cnt; //文件名指向
    char user_name[20]; //用户名
    char user_group[20]; //用户所属群组
    unsigned  int inode_size; //对应的文件大小
    int inode_dirblock[10]; //10个直接快 10*512B = 5120B
    int inode_indirect_block_first; //第一次间址

};



#endif //UNIXFILESYSTEM_INODE_H
