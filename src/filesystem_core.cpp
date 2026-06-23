#include <cstring>
#include "filesystem.h"

FileSystem::FileSystem(Image &_image, SuperBlock *superBlock1) : storage(_image, superBlock1) {
    this->state.nextGID = 0;
    this->state.nextUID = 0;
    this->is_login = false;
    strcpy(this->state.cur_user_name, "root");
    strcpy(this->state.cur_grop_name, "root");
    state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(state.cur_dir_name, "/");
}
