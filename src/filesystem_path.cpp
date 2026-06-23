#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

/*
 * 在指定目录 inode 下查找并进入某个子目录。
 *
 * 参数：
 *   inode_addr：起始目录的 inode 地址，通常是当前目录或根目录的 inode 地址。
 *   name：要查找的目录名，例如 "home"、".."、"."。
 *
 * 核心逻辑：
 *   1. 根据 inode_addr 读取父目录 inode。
 *   2. 遍历该目录 inode 的 10 个直接数据块 inode_dirblock[]。
 *   3. 每个目录数据块保存 16 个 Dir 目录项，因此最多扫描 10 * 16 = 160 个目录项。
 *   4. 在目录项中查找名字等于 name 的项。
 *   5. 找到后，根据目录项中的 inode_addr 读取目标 inode。
 *   6. 如果目标 inode 是目录，则检查当前用户是否有进入权限；如果不是目录，则继续查找。
 *   7. 权限检查通过后，更新当前目录状态：
 *      - state.cur_dir_addr 改为目标目录 inode 地址；
 *      - state.cur_dir_name 根据 "."、".." 或普通目录名进行更新。
 *
 * 注意：
 *   这个函数不只是“查找目录”，它找到目录后会直接改变当前工作目录状态，
 *   相当于 shell 中 cd 命令背后的核心路径切换逻辑。
 */
void FileSystem::FindDir(int inode_addr, const char *name) {
    iNode cur;
    FILE* fr = storage.image.get_file_read();
    fseek(fr, inode_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr); // 读出inode
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
                        state.cur_dir_name[k]='\0'; // 从后往前截断
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
