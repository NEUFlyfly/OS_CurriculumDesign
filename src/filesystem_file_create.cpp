//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

bool FileSystem::Create(int father_inode_addr, const char *name, char *file_content) {
    if(strlen(name) >= MAX_NAME_SIZE){
        cout << "超过最大文件名" << endl;
        return false;
    }
    FILE *fr = storage.image.get_file_read();
    FILE *fw = storage.image.get_file_write();
    Dir dir_vec[16];

    //从这个地址取出inode
    iNode cur;
    fseek(fr, father_inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    int find_pos_i = -1, find_pos_j = -1;
    for(int dir_block_num = 0; dir_block_num < 10; dir_block_num++){

        if(cur.inode_dirblock[dir_block_num]==-1){
            if(find_pos_i == -1) {
                find_pos_i = dir_block_num;
                find_pos_j = 0;
            }
            continue;
        }
        fseek(fr, cur.inode_dirblock[dir_block_num], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){

            if( find_pos_i == -1 && strcmp(dir_vec[j].name, "")==0 ){
                //找到一个空闲记录，将新文件创建到这个位置
                find_pos_i = dir_block_num;
                find_pos_j = j;
            }
            else if(strcmp(dir_vec[j].name, name)==0 ){
                //同一目录下文件和目录共用命名空间，任何重名项都不能创建。
                cout << "文件已经存在" << endl;
                file_content[0] = '\0';
                return false;
            }
        }

    }
    if(find_pos_i != -1){
        bool needs_new_parent_block = cur.inode_dirblock[find_pos_i] == -1;
        if(needs_new_parent_block) {
            int parent_block_addr = BlockAlloc();
            if(parent_block_addr == -1){
                cout << "Block分配失败" << endl;
                return false;
            }
            cur.inode_dirblock[find_pos_i] = parent_block_addr;
            memset(dir_vec, 0, sizeof(dir_vec));
        }
        else {
            //取出之前那个空闲目录项对应的磁盘块
            fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
            fread(dir_vec, sizeof(dir_vec), 1, fr);
            fflush(fr);
        }

        //创建这个目录项
        strcpy(dir_vec[find_pos_j].name, name);
        int cur_inode_addr = INodeAlloc();
        if(cur_inode_addr == -1){
            cout << "INode 分配失败" << endl;
            return false;
        }
        dir_vec[find_pos_j].inode_addr = cur_inode_addr;

        //设置新条目的inode
        iNode p;
        p.inode_id = static_cast<unsigned short>((cur_inode_addr - INODE_START_ADDR) / storage.superBlock->s_INODE_SIZE);
        strcpy(p.user_name, state.cur_user_name);
        strcpy(p.user_group, state.cur_grop_name);
        p.inode_cnt = 1;	//只有一个文件指向


        int k;

        //将buf内容存到磁盘块
        int len = static_cast<int>(strlen(file_content));
        for(int k = 0; k < len; k += storage.superBlock->s_BLOCK_SIZE){
            int cur_block_Addr = BlockAlloc();
            if(cur_block_Addr == -1){
                cout << "Block分配失败" << endl;
                return false;
            }
            p.inode_dirblock[k/storage.superBlock->s_BLOCK_SIZE] = cur_block_Addr;
            fseek(fw, cur_block_Addr,SEEK_SET);
            fwrite(file_content+k, storage.superBlock->s_BLOCK_SIZE, 1, fw);
        }


        //对其他项赋值为-1
        for(k= len/storage.superBlock->s_BLOCK_SIZE+1; k<10;k++){
            p.inode_dirblock[k] = -1;
        }


        if( len == 0){	//长度为0的话也分给它一个block
            int cur_block_Addr = BlockAlloc();
            if(cur_block_Addr == -1){
                cout << "Block分配失败" << endl;
                return false;
            }
            p.inode_dirblock[k/storage.superBlock->s_BLOCK_SIZE] = cur_block_Addr;
            //写入到当前目录的磁盘块
            fseek(fw, cur_block_Addr, SEEK_SET);
            fwrite(file_content, storage.superBlock->s_BLOCK_SIZE, 1, fw);

        }
        p.inode_size = len;
        p.inode_indirect_block_first = -1;	//没使用一级间接块
        p.inode_mode = 0;
        p.inode_mode = MODE_FILE | FILE_DEF_PERMISSION;

        //将inode写入到申请的inode地址
        fseek(fw, cur_inode_addr, SEEK_SET);
        fwrite(&p, sizeof(iNode), 1, fw);

        //将当前目录的磁盘块写回
        fseek(fw, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fwrite(dir_vec, sizeof(dir_vec), 1, fw);

        //写回inode
        cur.inode_cnt++;
        fseek(fw, father_inode_addr, SEEK_SET);
        fwrite(&cur,sizeof(iNode), 1, fw);
        fflush(fw);
        return true;
    }
    else
        return false;
}

bool FileSystem::MakeFile(int addr, char *param, char *buffer) {


    FILE* fr = storage.image.get_file_read();
    Dir dir_vec[16];

    iNode cur, create_file_inode;
    fseek(fr, addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if(strcmp(state.cur_user_name, cur.user_name)==0 || strcmp(state.cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(state.cur_user_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( ((cur.inode_mode >> filemode >> 2) & 1) == 0) {
        cout << "Permission Dennied" << endl;
        return false;
    }



    int i = 0;
    while(i < 160){

        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[i/16], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){
            //当前是否有重名
            if(strcmp(dir_vec[j].name, param) ==0 ){

                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&create_file_inode, sizeof(iNode), 1, fr);
                if( ((create_file_inode.inode_mode >> 9) & 1) == 0){
                    cout << "该文件已经存在" << endl;
                    return false;
                }
            }
            i++;
        }
    }

    //文件不存在，创建一个空文件
    if( ((cur.inode_mode >> filemode >> 1) & 1) == 1){
        //可写。可以创建文件
        buffer[0] = '\0';
        return Create(addr, param, buffer);	//创建文件
    }
    else{
        cout << "Permission Dennied" << endl;
        return false;
    }
}
