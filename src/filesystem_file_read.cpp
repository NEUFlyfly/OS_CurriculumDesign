#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

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
