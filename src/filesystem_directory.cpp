//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::FindDir(int inode_addr, const char *name) {
    iNode cur;
    FILE* fr = storage.image.get_file_read();
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);
    int filemode;




    int i = 0;
    while(i < 160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int token_block = cur.inode_dirblock[i/16];
        fseek(fr, token_block, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //输出该磁盘块中的所有目录项
        for(int j = 0; j < 16; j++){
            if(strcmp(dir_vec[j].name, name)==0){
                iNode dir;
                //取出该目录项的inode，判断该目录项是目录还是文件
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&dir, sizeof(iNode),1,fr);

                if( ( (dir.inode_mode >> 9) & 1 ) == 1 ){
                    //找到该目录，判断是否具有进入权限
                    //root用户所有目录都可以查看
                    // 6 -> owner 3 ->group 0 ->other
                    if(strcmp(state.cur_user_name, dir.user_name) ==0 )
                        filemode = 6;
                    else if(strcmp(state.cur_grop_name, dir.user_group)==0)
                        filemode = 3;
                    else
                        filemode = 0;

                    if (strcmp(state.cur_user_name, "root") == 0) {

                    } else if( ((dir.inode_mode >> filemode >> 2) & 1) ==0  ){
                        cout << "Permission Dennied" << endl;
                        return ;
                    }

                    //找到该目录项，如果是目录，更换当前目录

                    this->state.cur_dir_addr = dir_vec[j].inode_addr;
                    if(strcmp(dir_vec[j].name, ".")==0){
                        //本目录，不动
                    }
                    else if(strcmp(dir_vec[j].name, "..")==0){
                        //上一次目录
                        int k;
                        for(k = strlen(state.cur_dir_name); k>=0; k--)
                             if(state.cur_dir_name[k]=='/')
                                break;
                        state.cur_dir_name[k]='\0';
                        if(strlen(state.cur_dir_name)==0) {
                            //根目录
                            state.cur_dir_name[0] = '/'; state.cur_dir_name[1] = '\0';
                        }
                    }
                    else{
                        if(state.cur_dir_name[strlen(state.cur_dir_name)-1]!='/')
                            strcat(state.cur_dir_name, "/");
                        strcat(state.cur_dir_name, dir_vec[j].name);
                    }
                    return ;
                }
                else{
                    //找到该目录项，如果不是目录，继续找
                }

            }

            i++;
        }

    }

    //没找到
    cout << "找不到该目录" << endl;
    return ;


}

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

    int i = 0;
    int find_pos_i = -1, find_pos_j = -1;
    // 160 -> 10 直接 * 16 = 160 个目录项
    while(i < 160) {
        int dir_in_block = i / 16;
        if(cur.inode_dirblock[dir_in_block] == -1) {
            i += 16;
            continue;
        }

        fseek(fr, cur.inode_dirblock[dir_in_block], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j = 0; j < 16; j++) {
            if(strcmp(dir_vec[j].name, name) == 0) {
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if ((( helper.inode_mode >> 9) & 1 )) {
                    cout << "目录已存在" << endl;
                    return false;
                }
            } else {
                if(strcmp(dir_vec[j].name, "") == 0) {
                    if(find_pos_i == -1) {
                        find_pos_i = dir_in_block;
                        find_pos_j = j;
                    }
                }
            }
        }
        i++;
    }

    //找到空闲位置
    if(find_pos_i != -1){

        //取出这个直接块，要加入的目录条目的位置
        fseek(fr, cur.inode_dirblock[find_pos_i], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

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

void FileSystem::Chmod(int father_inode_addr, const char *name, int mode) {

    if(strcmp(name,".")==0 || strcmp(name,"..")==0){
        cout << "usage: chmod [filename] [permissions] : Change the file permissions" << endl;
        return ;
    }

    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    iNode cur, res;
    fseek(fr, father_inode_addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    int i = 0, j = 0;
    Dir dir_vec[16] = {0};
    bool flag = false;
    while(i < 160) {
        if(cur.inode_dirblock[i/16] == -1){
            i += 16;
            continue;
        }
        //取出磁盘块
        int alloc_block_dir = cur.inode_dirblock[i/16];
        fseek(fr, alloc_block_dir, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for(j = 0; j < 16; j++){
            if( strcmp(dir_vec[j].name, name)==0 ){	//找到该目录或者文件
                //取出对应的inode
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&res, sizeof(iNode),1,fr);
                fflush(fr);
                flag = true;
            }
            if(flag) break;
        }
        if(flag) break;
        i++;
    }
    if(i >= 160){
        cout << "当前文件名不存在" << endl;
        return ;
    }

    if(strcmp(state.cur_user_name, res.user_name) != 0 && strcmp(state.cur_user_name, "root")!=0){
        cout << "" << endl;
        return ;
    }


    res.inode_mode  = static_cast<unsigned short>((res.inode_mode >> 9 << 9) | mode);

    fseek(fw, dir_vec[j].inode_addr,SEEK_SET);
    fwrite(&res, sizeof(iNode), 1, fw);
    fflush(fw);
}

void FileSystem::readFirst(int addr) {
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    iNode cur;

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);

    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int file_mode;
    if(strcmp(state.cur_user_name, cur.user_name)==0 || strcmp(state.cur_user_name, "root") == 0)
        file_mode = 6;
    else if(strcmp(state.cur_grop_name, cur.user_group)==0)
        file_mode = 3;
    else
        file_mode = 0;


    if (strcmp(state.cur_user_name, "root") == 0) {

    } else if( ((cur.inode_mode >> file_mode >> 2) & 1) ==0  ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < cnt && i<160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        int allock_block_addr = cur.inode_dirblock[i/16];
        fseek(fr,allock_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j=0; j < 16 && i < cnt;j++){

            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            fflush(fr);

            if( strcmp(dir_vec[j].name, "")==0 ){
                continue;
            }

            if(i > 2 && (strcmp(dir_vec[j].name, ".")==0 || strcmp(dir_vec[j].name, "..")== 0) ) {
                continue;
            }

//            //输出信息
//            if( ( (helper.inode_mode>9) & 1 ) == 1 ){
//                cout << "d";
//            }
//            else{
//                cout << "-";
//            }


            int permiss_index = 8;
            while(permiss_index >= 0){
                if( ((helper.inode_mode >> permiss_index) & 1) == 1){
//                    if(permiss_index % 3 == 2)	cout << "r";
//                    if(permiss_index % 3 == 1)	cout << "w";
//                    if(permiss_index % 3 == 0)	cout << "x";
                }
                else{
//                    cout << "-";
                }
                permiss_index--;
            }
//            printf("\t");
//
//            printf("%d\t", helper.inode_cnt);	//该文件链接
//            printf("%s\t",helper.user_name);	//文件所属用户名
//            printf("%s\t",helper.user_group);	//文件所属用户名
//            printf("%5d B\t", helper.inode_size);	//文件大小
//            printf("%s", dir_vec[j].name);	//文件名
//            cout << endl;
            i++;
        }

    }
}

void FileSystem::ShowDir(int addr) {
    readFirst(addr);
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();
    iNode cur;

    fseek(fr, addr, SEEK_SET);
    fread(&cur,sizeof(iNode), 1, fr);
    fflush(fr);


    //取出目录项数
    int cnt = cur.inode_cnt;

    //判断文件模式。6为owner，3为group，0为other
    int file_mode;
    if(strcmp(state.cur_user_name, cur.user_name)==0 || strcmp(state.cur_user_name, "root") == 0)
        file_mode = 6;
    else if(strcmp(state.cur_grop_name, cur.user_group)==0)
        file_mode = 3;
    else
        file_mode = 0;


    if (strcmp(state.cur_user_name, "root") == 0) {

    } else if( ((cur.inode_mode >> file_mode >> 2) & 1) ==0  ){
        cout << "Permission Dennied" << endl;
        return ;
    }

    //依次取出磁盘块
    int i = 0;
    while(i < cnt && i<160){
        Dir dir_vec[16] = {0};
        if(cur.inode_dirblock[i/16]==-1){
            i+=16;
            continue;
        }
        int allock_block_addr = cur.inode_dirblock[i/16];
        fseek(fr,allock_block_addr, SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for(int j=0; j < 16 && i < cnt;j++){

            iNode helper;
            //取出该目录项的inode，判断该目录项是目录还是文件
            fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
            fread(&helper, sizeof(iNode),1,fr);
            fflush(fr);

            if( strcmp(dir_vec[j].name, "")==0 ){
                continue;
            }

            if(i > 2 && (strcmp(dir_vec[j].name, ".")==0 || strcmp(dir_vec[j].name, "..")== 0) ) {
                continue;
            }

            //输出信息
            if( ( (helper.inode_mode>>9) & 1 ) == 1 ){
                cout << "d";
            }
            else{
                cout << "-";
            }


            int permiss_index = 8;
            while(permiss_index >= 0){
                if( ((helper.inode_mode >> permiss_index) & 1) == 1){
                    if(permiss_index % 3 == 2)	cout << "r";
                    if(permiss_index % 3 == 1)	cout << "w";
                    if(permiss_index % 3 == 0)	cout << "x";
                }
                else{
                    cout << "-";
                }
                permiss_index--;
            }
            printf("\t");

            printf("%d\t", helper.inode_cnt);	//该文件链接
            printf("%s\t",helper.user_name);	//文件所属用户名
            printf("%s\t",helper.user_group);	//文件所属用户名
            printf("%5d B\t", helper.inode_size);	//文件大小
            printf("%s", dir_vec[j].name);	//文件名
            cout << endl;
            i++;
        }

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
