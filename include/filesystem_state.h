//
// Created by 林智超 on 2019-06-17.
//

#ifndef UNIXFILESYSTEM_FILESYSTEM_STATE_H
#define UNIXFILESYSTEM_FILESYSTEM_STATE_H

#include "const.h"

struct FileSystemState {
    int cur_dir_addr;
    int nextUID;
    int nextGID;
    char cur_user_name[MAX_NAME_SIZE];
    char cur_grop_name[MAX_NAME_SIZE];
    char cur_dir_name[MAX_NAME_SIZE];
    char cur_user_dir_name[MAX_NAME_SIZE];
};

#endif //UNIXFILESYSTEM_FILESYSTEM_STATE_H
