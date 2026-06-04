//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::DelFile(int addr, char *param) {
    if(strlen(param) >= MAX_NAME_SIZE){
        cout << "超过最长文件名" << endl;
        return ;
    }
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    iNode cur;
    fseek(fr, addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if(strcmp(state.cur_user_name, cur.user_name)==0 || strcmp(state.cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(state.cur_grop_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( ((cur.inode_mode >> filemode >> 1 ) & 1) ==0 ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < 160){	//小于160
        Dir dir_vec[16] = {0};

        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int allocat_block_addr = cur.inode_dirblock[i/16];
        fseek(fr, allocat_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        for(int j = 0; j < 16; j++){
            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            if( strcmp(dir_vec[j].name, param)==0){
                if( ( (helper.inode_mode >> 9) & 1 ) == 1 ){
                    //目录
                }
                else{
                    for(int k = 0; k < 10; k++)
                        if(helper.inode_dirblock[k] != -1)
                            FreeBlock(helper.inode_dirblock[k]);

                    //释放inode
                    INodeFree(dir_vec[j].inode_addr);

                    //删除该目录条目，写回磁盘
                    strcpy(dir_vec[j].name, "");
                    dir_vec[j].inode_addr = -1;
                    fseek(fw, allocat_block_addr, SEEK_SET);
                    fwrite(&dir_vec, sizeof(dir_vec), 1, fw);
                    cur.inode_cnt--;
                    fseek(fw, addr, SEEK_SET);
                    fwrite(&cur, sizeof(iNode), 1, fw);

                    fflush(fw);
                    return ;
                }
            }
            i++;
        }

    }
    cout << param << ": No such file or directory" << endl;
}
