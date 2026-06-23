#include <iostream>
#include <cstring>
#include <cstdio>
#include <fstream>
#include "filesystem.h"

void FileSystem::editor(int cur_dir_addr, char file_name[], char buf[], int buf_size)
{
    auto fr = storage.image.get_file_read();

    if (strlen(file_name) >= MAX_NAME_SIZE) {
        cout << "The filename is too long." << endl;
        return;
    }

    //清空缓冲区
    memset(buf, 0, buf_size);
    int maxlen = 0;	//到达过的最大长度

    //查找有无同名文件，有的话进入编辑模式，没有进入创建文件模式
    Dir dir_vector[16] = { 0 };	//临时目录清单

    //从这个地址取出inode
    iNode cur = { 0 }, fileInode = { 0 };
    fseek(fr, cur_dir_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    //判断文件模式。6为owner，3为group，0为other
    int filemode;
    if (strcmp(state.cur_user_name, cur.user_name) == 0 || strcmp(state.cur_user_name, "root") == 0)
        filemode = 6;
    else if (strcmp(state.cur_grop_name, cur.user_group) == 0)
        filemode = 3;
    else
        filemode = 0;

    int i = 0, j;
    int dno;
    int fileInodeAddr = -1;	//文件的inode地址
    bool isExist = false;	//文件是否已存在
    while (i<160)
    {
        //160个目录项之内，可以直接在直接块里找
        dno = i / 16;    //在第几个直接块里

        if (cur.inode_dirblock[dno] == -1)
        {
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dno], SEEK_SET);
        fread(dir_vector, sizeof(dir_vector), 1, fr);
        fflush(fr);

        //输出该磁盘块中的所有目录项
        for (j = 0; j < 16; j++)
        {
            if (strcmp(dir_vector[j].name, file_name) == 0)
            {
                //重名，取出inode，判断是否是文件
                fseek(fr, dir_vector[j].inode_addr, SEEK_SET);
                fread(&fileInode, sizeof(iNode), 1, fr);
                if (((fileInode.inode_mode >> 9) & 1) == 0)
                {    //是文件且重名，打开这个文件，并编辑
                    fileInodeAddr = dir_vector[j].inode_addr;
                    isExist = true;
                    break;
                }
            }
            i++;
        }
        if(isExist)
            break;
    }

    if (isExist) {

        if (((fileInode.inode_mode >> filemode >> 2) & 1) == 0) {
            //不可读
            cout << "Permission denied." << endl;
            return;
        }

        //将文件内容读取出来，显示在，窗口上
        int cnt = 0;
        int sumlen = fileInode.inode_size;	//文件长度
        int getlen = 0;	//取出来的长度
        for (int i = 0; i< 10; i++) {
            char fileContent[1000] = { 0 };
            if (fileInode.inode_dirblock[i] == -1) {
                continue;
            }
            //依次取出磁盘块的内容
            fseek(fr, fileInode.inode_dirblock[i], SEEK_SET);
            fread(fileContent, storage.superBlock->s_BLOCK_SIZE, 1, fr);	//读取出一个磁盘块大小的内容
            fflush(fr);
            //输出字符串
            int curlen = 0;	//当前指针
            while (curlen < storage.superBlock->s_BLOCK_SIZE) {
                if (getlen >= sumlen)	//全部输出完毕
                    break;
                buf[cnt++] = fileContent[curlen];	//输出到buf
                curlen++;
                getlen++;
            }
            buf[cnt++] = 0;
            if (getlen >= sumlen)
                break;
        }

        maxlen = sumlen;
    }

    fstream  bfile;
    bfile.open("bfile.txt", ios::out);


    Page page(buf);
    int curx, cury;
    int c;
    int beg = 0;
    int end = LINES - 2;
    bfile << end;
    bfile.close();
    int y_offset = 0;
    int tab_offset = 0;

    page.debug();

    initscr();
    clear();
    noecho();
    keypad(stdscr, true);
    page.print_page(beg, end);
    getyx(stdscr, cury, curx);

    while(true) {
        int oldy, oldx; getyx(stdscr, oldy, oldx);

        attron(A_REVERSE);
        move(LINES - 1, 0);
        clrtoeol();
        printw("x: %d y: %d o: %d  F4:Exit F5:Save&Exit", curx, cury, y_offset);;
        attroff(A_REVERSE);
        move(oldy, oldx);

        beg = 0 + y_offset;
        end = LINES - 2 + y_offset;

        c = getch();
        switch (c) {
            case KEY_F(4):
                clear();
                echo();
                keypad(stdscr, false);
                endwin();
                return;
            case KEY_F(5):
                page.save_to_buf(buf);
                clear();
                echo();
                keypad(stdscr, false);
                endwin();
                if(isExist) {
                    if (((fileInode.inode_mode >> filemode >> 1) & 1) == 1) {	//可写
                        WriteFile(fileInode, fileInodeAddr, buf);
                    }
                    else {	//不可写
                        cout << "Permission denied." << endl;
                    }
                } else {
                    if (((cur.inode_mode >> filemode >> 1) & 1) == 1) {
                        //可写。可以创建文件
                        Create(cur_dir_addr, file_name, buf);	//创建文件
                    }
                    else {
                        cout << "Permission denied." << endl;
                        return;
                    }
                }
                return;
            case KEY_UP:
                move_up(page, curx, cury, y_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_DOWN:
                move_down(page, curx, cury, y_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_LEFT:
                move_left(curx, cury);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_RIGHT:
                move_right(page, curx, cury, y_offset, tab_offset);
                page.print_page(beg, end);
                move(cury, curx);
                break;
            case KEY_DC:
            case 127:
            case KEY_BACKSPACE:
                if(strlen(page.text[cury + y_offset].line) == 0) {
                    page.remove_line(cury + y_offset);
                    move_up(page, curx, cury, y_offset);
                } else if( curx > 1 ) {
                    page.text[cury + y_offset].remove_char(curx - 2);
                    move_left(curx, cury);
                }
                page.print_page(beg, end);
                move(cury, curx);
                page.debug();
                break;
            case '\t':
                for(int i = 0; i < TAB_WIDTH; i++)
                {
                    page.text[cury + y_offset].insert_char(' ', curx - 1);
                    page.print_page(beg, end);
                    move_right(page, curx, cury, y_offset, tab_offset);
                }
                break;
            case '\n':
                page.insert_line(cury + y_offset + 1);
                page.print_page(beg, end);
                move_down(page, curx, cury, y_offset);
                break;
            default: // all other chars
                if(isprint(c)) {
                    page.text[cury + y_offset].insert_char(c, curx - 1);
                    page.print_page(beg, end);
                    move_right(page, curx, cury, y_offset, tab_offset);
                }
        }
    }

}
