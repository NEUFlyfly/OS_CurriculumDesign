#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

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
    while(i < cnt && i<160){ // 仅需检查前已经连接到了的目录项，超过部分不输出
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
    // readFirst(addr);
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
