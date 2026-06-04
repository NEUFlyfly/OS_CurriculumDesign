//
// Created by thankod on 19-6-26.
//

#ifndef UNIXFILESYSTEM_EDITOR_H
#define UNIXFILESYSTEM_EDITOR_H
#include <sys/stat.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <string>
#include <vector>
#include "const.h"
using namespace std;

#define LINE_SIZE 128
#define TAB_WIDTH 4

struct Line
{
    char *line;
    int size;
    Line();
    void clear();
    void insert_char(char c, int index);
    void expand();
    void add_char(char c);
    void remove_char(int index);
    ~Line();
};

struct Page
{

    Line *text;
    int numlines;
    int size;
    Page(char* buf);
    void dest_page();
    void insert_line(int index);
    void remove_line(int index);
    void expand();
    void print_page(int start, int end);
    void save_to_buf(char* buf);
    void debug();
} ;


void move_right(Page &page, int &curx, int &cury, int &y_offset, int &tab_offset);

void move_down(Page &page, int &curx, int &cury, int &y_offset);

void move_up(Page &page, int &curx, int &cury, int &y_offset);

void move_left(int& curx, int& cury);

#endif //UNIXFILESYSTEM_EDITOR_H
