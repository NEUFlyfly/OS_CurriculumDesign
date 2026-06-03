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

    int i = 0;
    int find_pos_i = -1, find_pos_j = -1;
    int dir_block_num;
    while(i < 160){
        dir_block_num = i/16;

        if(cur.inode_dirblock[dir_block_num]==-1){
            i += 16;
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
                //重名，取出inode，判断是否是文件
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if( ((helper.inode_mode>>9) &1)==0 ){	//是文件且重名，不能创建文件
                    cout << "文件已经存在" << endl;
                    file_content[0] = '\0';
                    return false;
                }
            }
            i++;
        }

    }
    if(find_pos_i != -1){
        //取出之前那个空闲目录项对应的磁盘块
        fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

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

void FileSystem::MakeFile(int addr, char *param, char *buffer) {


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
        return ;
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
                    return ;
                }
            }
            i++;
        }
    }

    //文件不存在，创建一个空文件
    if( ((cur.inode_mode >> filemode >> 1) & 1) == 1){
        //可写。可以创建文件
        buffer[0] = '\0';
        Create(addr, param, buffer);	//创建文件
    }
    else{
        cout << "Permission Dennied" << endl;
        return ;
    }
}

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

void FileSystem::Cat(int inode_addr, char name[])
{
    FILE* fr = storage.image.get_file_read();
    //清空缓冲区
    char buf[FILE_BUFFER] = { 0 };

    //查找有无同名文件
    Dir dir_vec[16] = { 0 };

    //从这个地址取出inode
    iNode cur = { 0 }, file_inode = { 0 };
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if (strcmp(state.cur_user_name, cur.user_name) == 0)
        filemode = 6;
    else if (strcmp(state.cur_grop_name, cur.user_group) == 0)
        filemode = 3;
    else
        filemode = 0;

    size_t i = 0, j;
    int dir_id;
    bool isExist = false;	//文件是否已存在
    bool label = false; // 是否停止
    while (i < 160) {
        //160个目录项之内，可以直接在直接块里找
        dir_id = i / 16;	//在第几个直接块里
        if (cur.inode_dirblock[dir_id] == -1) {
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dir_id], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for (j = 0; j < 16; j++) {
            if (strcmp(dir_vec[j].name, name) == 0) {
                //重名，取出inode，判断是否是文件
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&file_inode, sizeof(iNode), 1, fr);
                if (((file_inode.inode_mode >> 9) & 1) == 0) {	//是文件且重名，打开这个文件，并编辑
                    isExist = true;
                    label = true;
                }
            }
            if(label) break;
            i++;
        }
        if(label) break;
    }
    int cnt = 0;
    if (isExist) {	//文件已存在，输出文件内容

        //权限判断。判断文件是否可读
        if (((file_inode.inode_mode >> filemode >> 2) & 1) == 0) {
            //不可读
            cout << "Permission denied." << endl;
            return;
        }

        //将文件内容读取出来，显示在，窗口上
        i = 0;
        int sum_len = file_inode.inode_size;	//文件长度
        int len = 0;	//取出来的长度
        for (i = 0; i<10; i++) {
            char fileContent[1000] = { 0 };
            if (file_inode.inode_dirblock[i] == -1) {
                continue;
            }
            //依次取出磁盘块的内容
            fseek(fr, file_inode.inode_dirblock[i], SEEK_SET);
            fread(fileContent, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            fflush(fr);
            //输出字符串
            int cur_len = 0;
            while (cur_len < storage.superBlock->s_BLOCK_SIZE) {
                if (len >= sum_len)	//全部输出完毕
                    break;
                printf("%c", fileContent[cur_len]);	//输出到屏幕
                buf[cnt++] = fileContent[cur_len];	//输出到buf
                cur_len++;
                len++;
            }
            if (len >= sum_len)
                break;
        }
    }
    else {
        printf("cat %s : No such file\n", name);
    }
}
