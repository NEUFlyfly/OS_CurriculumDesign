//
// Created by thankod on 19-6-26.
//
#include "editor.h"
#include <fstream>

void move_left(int &curx, int &cury)
{
    if(curx - 1 > 0)
        move(cury, --curx);

}

void move_right(Page &page, int &curx, int &cury, int &y_offset, int &tab_offset)
{
    if(curx <= strlen(page.text[cury + y_offset].line))
    {
        if(page.text[cury + y_offset].line[curx + tab_offset] == '\t') {
            move(cury, ++curx);
        } else {
            move(cury, ++curx);
        }
    }
}

void move_down(Page &page, int &curx, int &cury, int &y_offset)
{
    if(cury < LINES - 2 - 1 && cury < page.numlines - 1) {
        ++cury;
    } else if (cury + y_offset < page.numlines - 1) {
        ++(y_offset);
        page.print_page(0 + y_offset, LINES - 2 + y_offset);
    }
    if(curx > strlen(page.text[cury + y_offset].line) + 1 )
        curx = strlen(page.text[cury + y_offset].line) + 1;
    move(cury, curx);
}

void move_up(Page &page, int &curx, int &cury, int &y_offset)
{
    if(cury > 0) {
        --cury;
    } else if (y_offset > 0) {
        --(y_offset);
        page.print_page(0 + y_offset, LINES - 2 + y_offset);
    }
    if(curx > strlen(page.text[cury + y_offset].line) + 1 )
        curx = strlen(page.text[cury + y_offset].line) + 1;
    move(cury, curx);
}





Line::Line() {
    size = LINE_SIZE;
    line = new char[LINE_SIZE];
    line[0] = '\0';
}

void Line::insert_char(char c, int index) {
    int i;
    if(strlen(line) >= size - 2) expand();
    for(i = strlen(line); i >= index; i--)
        line[i + 1] = line[i];
    line[index] = c;
}

void Line::expand() {
    int new_size = size * 2;
    char *temp = new char[new_size];
    strcpy(temp, line);
    delete line;
    line = temp;
    size = new_size;
}

void Line::add_char(char c) {
    insert_char(c, strlen(line));
}

void Line::remove_char(int index) {
    int i;
    int len = strlen(line);
    for(i = index; i < len; i++)
        line[i] = line[i + 1];
}

Line::~Line()
{
    delete line;
}

void Line::clear()
{
    line[0] = '\0';
}


Page::Page(char *buf)
{
    text = new Line[500];
    this->size = 500;
    char ch = 1;
    int now = 0;
    numlines = 0;
    int len = strlen(buf);
    if(len == 0) {
        numlines = 1;
        return;
    }

    for(int line = 0; now != len && ch != '\0'; line++)
    {
        ch = buf[now++];
        while(ch != '\n' && ch != '\0')
        {
            Line *currline = &(text[line]);
            if(ch != '\t')
            {
                currline->add_char(ch);
            }
            else
            {
                for(int i = 0; i < TAB_WIDTH; i++)
                {
                    currline->add_char(' ');
                }
            }
            ch = buf[now++];
        }
        numlines++;
    }
}

void Page::dest_page()
{
    int i;
    for(i = 0; i < numlines; i++) {
        delete text[i].line;
    }
    delete text;
}

void Page::insert_line(int index)
{
    if(numlines >= size) expand();

    Line* newline = new Line;

    for(int i = numlines - 1; i >= index; i--)
        text[i + 1] = text[i];

    text[index] = *newline;
    numlines++;
}

void Page::remove_line(int index)
{
    if(numlines > 1)
    {
        delete text[index].line;
        for(int i = index; i < numlines - 1; i++)
        {
            text[i] = text[i + 1];
        }
        numlines--;
    }
}

void Page::expand()
{
    int new_size = size * 2;
    Line *newline = new Line[new_size];

    for(int i = 0; i < size; i++)
        newline[i] = text[i];

    text = newline;
    size = new_size;
}

void Page::print_page(int start, int end)
{
    int i = start;
    int line = 0;
    for(i = start, line = 0; i < numlines && i < end; i++, line++)
    {
        move(line, 0);
        clrtoeol();
        printw(" %s", text[i].line);
    }
    if(start < end)
    {
        move(line, 0);
        clrtoeol();
        move(line - 1, 1);
    }
    refresh();
}

void Page::save_to_buf(char *buf)
{
    int line, col;
    int now = 0;
    for(line = 0; line < numlines; line++)
    {
        col = 0;
        while(text[line].line[col] != '\0')
        {
            buf[now++] = text[line].line[col];
            col++;
        }
        if(line != numlines - 1)
            buf[now++] = '\n';
        else
            buf[now++] = '\n';
    }
    buf[now++] = '\0';

}

void Page::debug()
{
    fstream afile;
    afile.open("afile.txt", ios::out);

    for(int i = 0; i < numlines; i++) {
        afile << text[i].line << "\n";
    }
    afile << "\n\n\n";
    afile.close();

}



