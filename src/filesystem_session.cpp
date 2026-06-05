//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

bool FileSystem::Login() {
    char user_name[100] = {0};
    char pass_wd[100] = {0};
    cout << "username: ";
    cin >> user_name;
    cin.ignore(100, '\n');
    cout << "password: ";
    cin >> pass_wd;
    cin.ignore(100, '\n');
    if(check(user_name, pass_wd)) {
        is_login = true;
        return true;
    } else {
        is_login = false;
        return false;
    }

}

bool FileSystem::check(char name[], char passwd[]) {
    int user_inode_addr = -1;
    int passwd_inode_addr = -1;
    iNode user_inode;
    iNode passwd_inode;
    iNode cur_dir_inode;
    FILE* fr = storage.image.get_file_read();
    Dir dir_vec[16]; //临时目录
    //进入配置文件目录

    //找到user文件和passwd文件的inode地址

    //取出当前inode地址
    FindDir(this->state.cur_dir_addr, "etc");
    fseek(fr, this->state.cur_dir_addr, SEEK_SET);
    fread(&cur_dir_inode, sizeof(iNode), 1, fr);
    for(int i = 0; i < 10; i++) {
        if(cur_dir_inode.inode_dirblock[i] == -1) {
            continue;
        }
        fseek(fr, cur_dir_inode.inode_dirblock[i], SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);
        for(int j = 0; j < 16; j++) {
            if(strcmp(dir_vec[j].name, "user") == 0 || strcmp(dir_vec[j].name, "passwd") == 0) {
                iNode helper;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode), 1, fr);
                if( ((helper.inode_mode >> 9) & 1)==0 ){
                    //是文件
                    if( strcmp(dir_vec[j].name, "user")==0 ){
                        user_inode_addr = dir_vec[j].inode_addr;
                        user_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "passwd") == 0 ){
                        passwd_inode_addr = dir_vec[j].inode_addr;
                        passwd_inode = helper;
                    }
                }
            }
        }
        if(user_inode_addr != -1 && passwd_inode_addr != -1) {
            break;
        }
    }

    char user[10000];
    char buffer[600]; //存磁盘快内容
    int block_pointer = 0;
    int i;

    // Safety check: if user or passwd file not found, fail immediately
    if (user_inode_addr == -1 || passwd_inode_addr == -1) {
        cout << "用户配置文件缺失" << endl;
        FindDir(state.cur_dir_addr, "..");
        return false;
    }

    for(i = 0; i < user_inode.inode_size && i < 9999; i++) {
        if(i % storage.superBlock->s_BLOCK_SIZE == 0) {
            //需要用新的磁盘块
            fseek(fr, user_inode.inode_dirblock[i/(storage.superBlock->s_BLOCK_SIZE)], SEEK_SET);
            fread(buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            block_pointer = 0;
        }
        user[i] = buffer[block_pointer++];
    }
    user[i] = '\0';
    if(strstr(user, name) == nullptr) {
        cout << "用户不存在" << endl;
        FindDir(state.cur_dir_addr, "..");
        return false;
    }

    block_pointer = 0;
    for(i = 0; i < passwd_inode.inode_size && i < 9999; i++) {
        if(i % storage.superBlock->s_BLOCK_SIZE == 0) {
            //需要用新的磁盘块
            fseek(fr, passwd_inode.inode_dirblock[i/(storage.superBlock->s_BLOCK_SIZE)], SEEK_SET);
            fread(buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            block_pointer = 0;
        }
        user[i] = buffer[block_pointer++]; // passwd文件内容存到user里，复用变量，但其实不太好
    }
    user[i] = '\0';

    char *p;
    if( (p = strstr(user, name)) == NULL) {
        cout << "passwd文件中不存在该用户" << endl;
        FindDir(state.cur_dir_addr, "..");
        return false;
    }

    while ((*p) != ':') {
        p++;
    }
    p++;
    block_pointer = 0;
    while((*p) != '\n') {
        buffer[block_pointer++] = *p;
        p++;
    }
    buffer[block_pointer] = '\0';
    //核对密码
    if(strcmp(buffer, passwd)==0){	//密码正确，登陆
        strcpy(state.cur_user_name, name);
        if(strcmp(name,"root")==0)
            strcpy(state.cur_grop_name, "root");	//当前登陆用户组名
        else
            strcpy(state.cur_grop_name,"user");	//当前登陆用户组名
        FindDir(state.cur_dir_addr, "..");
        FindDir(state.cur_dir_addr, "home");
		FindDir(state.cur_dir_addr, name);	//进入到用户目录
        strcpy(state.cur_user_dir_name, state.cur_dir_name);	//复制当前登陆用户目录名
        return true;
    }
    else{
        cout << "密码错误"<< endl;
        FindDir(state.cur_dir_addr, "..");	//回到根目录
        return false;
    }
}

void FileSystem::Quit() {
    memset(state.cur_user_name, 0, sizeof(state.cur_user_name));		//清空当前用户名
    memset(state.cur_user_dir_name, 0, sizeof(state.cur_user_dir_name));	//清空当前用户目录
    state.cur_dir_addr = ROOT_DIR_ADDR;	//当前用户目录地址设为根目录地址
    strcpy(state.cur_dir_name, "/");		//当前目录设为"/"
    is_login = false;
}
