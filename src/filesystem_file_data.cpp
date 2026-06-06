//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::WriteFile(iNode inode, int inode_addr, char *buffer) {
    int len = strlen(buffer);
    for(int k = 0; k < len; k += storage.superBlock->s_BLOCK_SIZE){	//最多10次，10个磁盘快，即最多5K
        int cur_block_addr;
        if(inode.inode_dirblock[k/storage.superBlock->s_BLOCK_SIZE] == -1){
            cur_block_addr = BlockAlloc();
            if(cur_block_addr == -1){
                cout << "分配block出现错误" << endl;
                return ;
            }
            inode.inode_dirblock[k/storage.superBlock->s_BLOCK_SIZE] = cur_block_addr;
        }
        else{
            //直接覆盖
            cur_block_addr = inode.inode_dirblock[k / storage.superBlock->s_BLOCK_SIZE];
        }

        fseek(storage.image.get_file_write(), cur_block_addr, SEEK_SET);
        fwrite(buffer+k,storage.superBlock->s_BLOCK_SIZE,1, storage.image.get_file_write());
        fflush(storage.image.get_file_write());
    }
    //更新该文件大小
    inode.inode_size = len;
    fseek(storage.image.get_file_write(), inode_addr, SEEK_SET);
    fwrite(&inode, sizeof(iNode), 1, storage.image.get_file_write());
    fflush(storage.image.get_file_write());
}
