#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::DeleteUser(char *username) {
    if(strcmp(username, "root") == 0) {
        cout << "无法删除root 用户" << endl;
    }

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
    char helper_cur_user_dir_name[100];
    int  helper_cur_dir_addr;
    char helper_cur_dir_name[100];

    iNode cur_dir_inode;	//当前目录的inode
    int i,j;

    //保存现场
    strcpy(helper_cur_user_name, state.cur_user_name);
    strcpy(helper_cur_user_dir_name, state.cur_user_dir_name);
    helper_cur_dir_addr = state.cur_dir_addr;
    strcpy(helper_cur_dir_name, state.cur_dir_name);




    //返回根目录
    memset(state.cur_user_name, 0, sizeof(state.cur_user_name));
    memset(state.cur_user_dir_name, 0, sizeof(state.cur_user_dir_name));
    state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(state.cur_dir_name, "/");

    //进入用户目录
    FindDir(state.cur_dir_addr, "etc");


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



    if(strstr(buffer, username) == nullptr){
        cout << "用户不存在" << endl;
        strcpy( state.cur_user_name, helper_cur_user_name);
        strcpy(state.cur_user_dir_name, helper_cur_user_dir_name);
        state.cur_dir_addr = helper_cur_dir_addr;
        strcpy(state.cur_dir_name, helper_cur_dir_name);
        return ;
    }

    DeleteUserContent(buffer, username);
    user_inode.inode_size = strlen(buffer);
    WriteFile(user_inode, passwd_inode_Addr, buffer);

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
    DeleteUserContent(buffer, username);
    passwd_inode.inode_size = strlen(buffer);
    WriteFile(passwd_inode, passwd_inode_Addr, buffer);



    //取出group文件内容
    pointer = 0;
    for(int i = 0; i < group_inode.inode_size; i++){
        if(i% storage.superBlock->s_BLOCK_SIZE==0){	//超出了这个磁盘块
            //换新的磁盘块
            fseek(fr, group_inode.inode_dirblock[i/storage.superBlock->s_BLOCK_SIZE], SEEK_SET);
            fread(&buffer, storage.superBlock->s_BLOCK_SIZE, 1, fr);
            pointer = 0;
        }
        file_content[i] = buffer[pointer++];
    }
    buffer[group_inode.inode_size] = '\0';
    DeleteUserContent(buffer, username);
    group_inode.inode_size = strlen(buffer);
    WriteFile(group_inode, group_inode_Addr, buffer);




    char c, User_Dir_to_be_deleted[110] = { 0 }, cmp_Cur_Dir_Name[311];
    strcpy(User_Dir_to_be_deleted, "/home/");
    strcat(User_Dir_to_be_deleted, username);
    strcat(User_Dir_to_be_deleted, "/");

    strcpy(cmp_Cur_Dir_Name, helper_cur_dir_name);
    strcat(cmp_Cur_Dir_Name, "/");

    for (i = 0, j = 0; j < 3; i++) {
        c = cmp_Cur_Dir_Name[i];
        if (c != User_Dir_to_be_deleted[i])
            break;
        else if (c == '/')
            j++;
    }

    if (j != 3) {
        //恢复现场，回到原来的目录
        strcpy(state.cur_user_name,  "root");
        strcpy(state.cur_user_dir_name, "root");
        state.cur_dir_addr = ROOT_DIR_ADDR;
        strcpy(state.cur_dir_name, "/");
        FindDir(state.cur_dir_addr, "home");
        DeleteDir(state.cur_dir_addr, username);

        strcpy(state.cur_user_name, helper_cur_user_name);
        strcpy(state.cur_user_dir_name, helper_cur_user_dir_name);
        state.cur_dir_addr = helper_cur_dir_addr;
        strcpy(state.cur_dir_name, helper_cur_dir_name);
    }
    else {
        // 在root用户进入a用户目录，删掉a用户，则回到根目录
        strcpy(state.cur_user_name,  "root");
        strcpy(state.cur_user_dir_name, "root");
        state.cur_dir_addr = ROOT_DIR_ADDR;	//当前用户目录地址设为根目录地址
        strcpy(state.cur_dir_name, "/");		//当前目录设为"/"

        // 删除username目录
        FindDir(state.cur_dir_addr, "home");
        DeleteDir(state.cur_dir_addr, username);
        // 回到/home/root
        FindDir(state.cur_dir_addr, "..");
    }
    cout << "用户删除成功" << endl;

    return ;
}
