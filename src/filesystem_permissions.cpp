#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

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
