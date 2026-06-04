//
// Created by 林智超 on 2019-06-19.
//

#ifndef UNIXFILESYSTEM_DIR_H
#define UNIXFILESYSTEM_DIR_H

#include "const.h"



//目录项 size = 32 byte one block can store 512/32 = 16个目录项
struct Dir {
    char name[MAX_NAME_SIZE];
    int inode_addr;
};


#endif //UNIXFILESYSTEM_DIR_H
