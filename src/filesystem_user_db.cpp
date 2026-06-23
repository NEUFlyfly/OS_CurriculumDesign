#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::DeleteUserContent(char *buffer, char *username) {
    char* pointer = strstr(buffer, username);
    *pointer = '\0';
    while((*pointer) != '\n') {
        pointer++;
    }

    //pointer + 1 -> 下一个user的开始位置;
    pointer++;
    strcat(buffer, pointer);
}
