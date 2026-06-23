#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::AddUser(char *username) {



    // 取出文件指针
    FILE* fr = storage.image.get_file_read();
    FILE* fw = storage.image.get_file_write();

    if(strcmp(state.cur_user_name, "root")!=0){
        cout << "非root用户, 不能添加新用户" << endl;
        return ;
    }
    Dir dir_vec[16];
    int user_inode_Addr = -1;	//用户文件inode地址
    int passwd_inode_Addr = -1;	//用户密码文件inode地址
    int group_inode_Addr = -1;	//用户组文件inode地址
    iNode user_inode;		//用户文件的inode
    iNode passwd_inode;		//用户密码文件的inode
    iNode group_inode;		//用户组文件inode

    //原来的目录


    char helper_cur_user_name[100];
    char helper_cur_user_name_next[100];
    char helper_cur_user_dir_name[100];
    int  helper_cur_dir_addr;
    char helper_cur_dir_name[100];
    char helper_cur_group_name[100];

    iNode cur_dir_inode;	//当前目录的inode
    int i,j;

    strcpy(helper_cur_user_name, state.cur_user_name);
    strcpy(helper_cur_user_dir_name, state.cur_user_dir_name);
    helper_cur_dir_addr = state.cur_dir_addr;
    strcpy(helper_cur_dir_name, state.cur_dir_name);


    //返回根目录
    memset(state.cur_user_name, 0, sizeof(state.cur_user_name));
    memset(state.cur_user_dir_name, 0, sizeof(state.cur_user_dir_name));
    state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(state.cur_dir_name, "/");



    //跳转用户文件夹
    FindDir(state.cur_dir_addr, "home");

    //保存现场
    strcpy( helper_cur_user_name_next , state.cur_user_name);
    strcpy( helper_cur_group_name, state.cur_grop_name);


    //更改
    strcpy(state.cur_user_name, username);
    strcpy(state.cur_grop_name, "user");


    if(!MakeDir(state.cur_dir_addr, username)){

        strcpy( state.cur_user_name, helper_cur_user_name);
        strcpy( state.cur_grop_name, helper_cur_group_name);
        strcpy(state.cur_user_dir_name, helper_cur_user_dir_name);
        state.cur_dir_addr = helper_cur_dir_addr;
        strcpy(state.cur_dir_name, helper_cur_dir_name);
        cout << "用户注册失败" << endl;
        return ;
    }

    Chmod(state.cur_dir_addr, username, 0600);
    //恢复现场
    strcpy( state.cur_user_name, helper_cur_user_name_next);
    strcpy( state.cur_grop_name, helper_cur_group_name);

    //回到根目录
    memset(state.cur_user_name, 0, sizeof(state.cur_user_name));
    memset(state.cur_user_dir_name, 0, sizeof(state.cur_user_dir_name));
    state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(state.cur_dir_name, "/");

    //进入用户目录
    FindDir(state.cur_dir_addr, "etc");

    cout << "password: ";
    //用户密码
    char password[100] = {0};
    fflush(stdin);
    cin.getline(password, 100);



    fseek(fr, state.cur_dir_addr, SEEK_SET);
    fread(&cur_dir_inode, sizeof(iNode), 1, fr);


    for(int i = 0;i < 10; i++){
        if(cur_dir_inode.inode_dirblock[i] == -1){
            continue;
        }
        //在10个直接块中查询
        fseek(fr, cur_dir_inode.inode_dirblock[i], SEEK_SET);
        fread(&dir_vec, sizeof(dir_vec), 1, fr);

        //一个j里面存储16个文件夹
        for(int j = 0; j < 16; j++){
            if( strcmp(dir_vec[j].name,"user")==0 || strcmp(dir_vec[j].name, "passwd")==0 || strcmp(dir_vec[j].name, "group") == 0 ) {
                iNode helper;

                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&helper, sizeof(iNode),1,fr);


                //文件名标识别符号判断
                if( ((helper.inode_mode >> 9) & 1 ) ==0 ){

                    if( strcmp(dir_vec[j].name, "user")==0 ){
                        user_inode_Addr = dir_vec[j].inode_addr;
                        user_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "passwd") ==0 ){
                        passwd_inode_Addr = dir_vec[j].inode_addr;
                        passwd_inode = helper;
                    }
                    else if(strcmp(dir_vec[j].name, "group")==0 ){
                        group_inode_Addr = dir_vec[j].inode_addr;
                        group_inode = helper;
                    }
                }
            }
        }
        if(user_inode_Addr != -1 && passwd_inode_Addr != -1)
            break;
    }

    char file_content[10000];
    char buffer[600];



    int pointer = 0;
    for(int i = 0; i < user_inode.inode_size; i++){
        if(i % storage.superBlock->s_BLOCK_SIZE == 0){
            //换新的磁盘块
            fseek(fr, user_inode.inode_dirblock[i/storage.superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[user_inode.inode_size] = '\0';



    if(strstr(buffer, username)!= nullptr){
        cout << "用户已经存在" << endl;
        strcpy( state.cur_user_name, helper_cur_user_name);
        strcpy(state.cur_user_dir_name, helper_cur_user_dir_name);
        state.cur_dir_addr = helper_cur_dir_addr;
        strcpy(state.cur_dir_name, helper_cur_dir_name);
        return ;
    }

    // 1 -> 普通用户组 buffer + strlen(buffer) -> 定位到末尾
    sprintf(buffer + strlen(buffer), "%s:x:%d:%d\n", username, state.nextUID++, 1);
    user_inode.inode_size = strlen(buffer);
    WriteFile(user_inode, user_inode_Addr, buffer);


    pointer = 0;
    for(int i = 0;i < passwd_inode.inode_size;i++){
        if(i% storage.superBlock->s_BLOCK_SIZE==0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, passwd_inode.inode_dirblock[i/storage.superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[passwd_inode.inode_size] = '\0';

    sprintf(buffer + strlen(buffer),"%s:%s\n", username, password);
    passwd_inode.inode_size = strlen(buffer);
    WriteFile(passwd_inode, passwd_inode_Addr, buffer);


    //取出group文件内容
    pointer = 0;
    for(int i = 0; i < group_inode.inode_size; i++){
        if(i% storage.superBlock->s_BLOCK_SIZE == 0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, group_inode.inode_dirblock[i/storage.superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[group_inode.inode_size] = '\0';


    if(buffer[strlen(buffer)-2]==':')  {                    //当前组内无用户
        sprintf(buffer+strlen(buffer)-1, "%s\n", username);
    }
    else {
        sprintf(buffer+strlen(buffer)-1, ",%s\n",username);
    }
    group_inode.inode_size = strlen(buffer);
    WriteFile(group_inode, group_inode_Addr, buffer);

    //恢复现场，回到原来的目录
    strcpy(state.cur_user_name, helper_cur_user_name);
    strcpy(state.cur_user_dir_name, helper_cur_user_dir_name);
    state.cur_dir_addr = helper_cur_dir_addr;
    strcpy(state.cur_dir_name, helper_cur_dir_name);
    cout << "用户注册成功" << endl;
    return ;
}
