//
// Created by 林智超 on 2019-06-17.
//

#ifndef UNIXFILESYSTEM_FILESYSTEM_STORAGE_H
#define UNIXFILESYSTEM_FILESYSTEM_STORAGE_H

#include "const.h"
#include "image.h"
#include "superblock.h"

struct FileSystemStorage {
    FileSystemStorage(Image& _image, SuperBlock* _superBlock) : image(_image), superBlock(_superBlock) {}

    Image& image;
    SuperBlock* superBlock;
    bool inode_bitmap[INODE_NUM];
    bool block_bitmap[BLOCK_NUM];
};

#endif //UNIXFILESYSTEM_FILESYSTEM_STORAGE_H
