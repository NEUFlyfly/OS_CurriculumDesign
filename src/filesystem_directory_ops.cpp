#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

bool FileSystem::MakeDir(int inode_addr, char *name) {
    if(strlen(name) >= MAX_NAME_SIZE) {
        cout << "超过最大目录长度" << endl;
        return false;
    }
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    Dir dir_vec[16];
    iNode cur;
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);
    int filemode;

    if(strcmp(state.cur_user_name, cur.user_name)==0 || strcmp(state.cur_user_name, "root") == 0)
        filemode = 6;
    else if(strcmp(state.cur_grop_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if (strcmp(state.cur_user_name, "root") != 0 && ((cur.inode_mode >> filemode >> 2) & 1) == 0) {
        cout << "Permission Dennied" << endl;
        return false;
    }

    int find_pos_i = -1, find_pos_j = -1;
    // 160 -> 10 直接 * 16 = 160 个目录项
    for(int dir_in_block = 0; dir_in_block < 10; dir_in_block++) {
        if(cur.inode_dirblock[dir_in_block] == -1) {
            if(find_pos_i == -1) {
                find_pos_i = dir_in_block;
                find_pos_j = 0;
            }
            continue;
        }

        fseek(fr, cur.inode_dirblock[dir_in_block], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j = 0; j < 16; j++) {
            if(strcmp(dir_vec[j].name, name) == 0) {
                cout << "目录项已存在" << endl;
                return false;
            } else {
                if(strcmp(dir_vec[j].name, "") == 0) {
                    if(find_pos_i == -1) {
                        find_pos_i = dir_in_block;
                        find_pos_j = j;
                    }
                }
            }
        }
    }

    //找到空闲位置
    if(find_pos_i != -1){

        bool needs_new_parent_block = cur.inode_dirblock[find_pos_i] == -1;
        if(needs_new_parent_block) {
            int parent_block_addr = BlockAlloc();
            if(parent_block_addr == -1) {
                printf("block分配失败\n");
                return false;
            }
            cur.inode_dirblock[find_pos_i] = parent_block_addr;
            memset(dir_vec, 0, sizeof(dir_vec));
        }
        else {
            //取出这个直接块，要加入的目录条目的位置
            fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
            fread(dir_vec, sizeof(dir_vec), 1, fr);
            fflush(fr);
        }

        //创建这个目录项
        strcpy(dir_vec[find_pos_j].name, name);	//目录名


        //写入两条记录 "." ".."，分别指向当前inode节点地址，和父inode节点
        int cur_inode_addr = INodeAlloc();	//分配当前节点地址
        if(cur_inode_addr == -1){
            cout << "inode节点不够" << endl;
            return false;
        }
        dir_vec[find_pos_j].inode_addr = cur_inode_addr; //给这个新的目录分配的inode地址

        //设置新的inode
        iNode p;
        p.inode_id = static_cast<unsigned short>((cur_inode_addr - INODE_START_ADDR) / storage.superBlock->s_INODE_SIZE);
        strcpy(p.user_name, state.cur_user_name);
        strcpy(p.user_group, state.cur_grop_name);
        p.inode_cnt = 2;	//两个项，当前目录, "."和".."

        //分配这个inode的磁盘块，在磁盘号中写入两条记录 . 和 ..
        int curblockAddr = BlockAlloc();
        if(curblockAddr == -1){
            printf("block分配失败\n");
            return false;
        }
        Dir dir_vec_next[16] = {0};	//临时目录项列表 - 2
        strcpy(dir_vec_next[0].name, ".");
        strcpy(dir_vec_next[1].name, "..");
        dir_vec_next[0].inode_addr = cur_inode_addr;	//当前目录inode地址
        dir_vec_next[1].inode_addr = inode_addr;	//父目录inode地址

        //写入到当前目录的磁盘块
        fseek(fw, curblockAddr,SEEK_SET);
        fwrite(dir_vec_next, sizeof(dir_vec_next), 1, fw);

        p.inode_dirblock[0] = curblockAddr;
        for(int k = 1; k < 10; k++){
            p.inode_dirblock[k] = -1;
        }
        p.inode_size = storage.superBlock->s_BLOCK_SIZE;
        p.inode_indirect_block_first = -1;	//没使用一级间接块
        p.inode_mode = MODE_DIR | DIR_DEFAULT_PERMISSION;

        //将inode写入到申请的inode地址
        fseek(fw, cur_inode_addr, SEEK_SET);
        fwrite(&p, sizeof(iNode), 1, fw);

        //将当前目录的磁盘块写回
        fseek(fw, cur.inode_dirblock[find_pos_i],SEEK_SET);
        fwrite(dir_vec, sizeof(dir_vec), 1, fw);

        //写回inode
        cur.inode_cnt++;
        fseek(fw, inode_addr, SEEK_SET);
        fwrite(&cur,sizeof(iNode), 1, fw);
        fflush(fw);
        return true;
    }
    else{
        cout << "无空闲目录项" << endl;
        printf("没找到空闲目录项,目录创建失败");
        return false;
    }
}

bool FileSystem::DeleteDir(int addr, char *params) {

    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();

    if(strlen(params) >= MAX_NAME_SIZE){
        cout << "已经超过最长目录长度" << endl;
        return false;
    }
    if(strcmp(params, ".")==0 || strcmp(params, "..")==0){
        cout << "不能删除当前目录或者上级目录" << endl;
        return 0;
    }

    //从这个地址取出inode
    iNode cur;
    fseek(fr, addr,SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if( strcmp(state.cur_user_name, cur.user_name)==0 )
        filemode = 6;
    else if(strcmp(state.cur_user_name, cur.user_group)==0)
        filemode = 3;
    else
        filemode = 0;

    if( (((cur.inode_mode >> filemode >> 1) & 1 ) ==0 ) && (strcmp(state.cur_user_name, "root")!=0) ){
        cout << "Permission Dennied" << endl;
        return false;
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
        int alloact_block_addr = cur.inode_dirblock[i/16];
        fseek(fr, alloact_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //找到要删除的目录
        for(int j = 0; j < 16; j++){
            iNode helper;

            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode), 1, fr);

            if( strcmp(dir_vec[j].name, params) == 0){
                if( ( (helper.inode_mode >> 9) & 1 ) == 1 ){
                    //目录
                    DeleteFileOrDir(dir_vec[j].inode_addr);
                    strcpy(dir_vec[j].name, "");
                    dir_vec[j].inode_addr = -1;
                    fseek(fw, alloact_block_addr, SEEK_SET);
                    fwrite(&dir_vec, sizeof(dir_vec),1,fw);
                    cur.inode_cnt = static_cast<unsigned short>(cur.inode_cnt - 1);
                    fseek(fw, addr,SEEK_SET);
                    fwrite(&cur,sizeof(iNode), 1, fw);
                    fflush(fw);
                    return true;
                }
                else{
                    //非目录
                }
            }
            i++;
        }

    }


    return false;
}

void FileSystem::DeleteFileOrDir(int addr) {
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    iNode cur;
    fseek(fr, addr,SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //取出目录项数
    int cnt = cur.inode_cnt;
    if(cnt <= 2){
        FreeBlock(cur.inode_dirblock[0]);
        INodeFree(addr);
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < 160){	//小于160
        Dir dir_vec[16] = {0};

        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        //取出磁盘块
        int allocate_block = cur.inode_dirblock[i/16];
        fseek(fr, allocate_block,SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //从磁盘块中依次取出目录项，递归删除
        bool isFree = false;
        for(int j = 0; j < 16; j++){

            if(! (strcmp(dir_vec[j].name, ".")==0 ||
                  strcmp(dir_vec[j].name, "..")==0 ||
                  strcmp(dir_vec[j].name, "")==0 ) ){
                isFree = true;
                DeleteFileOrDir(dir_vec[j].inode_addr);
            }
            i++;
        }

        //该磁盘块已空，回收
        if(isFree)
            FreeBlock(allocate_block);

    }
    //该inode已空，回收
    INodeFree(addr);
}
