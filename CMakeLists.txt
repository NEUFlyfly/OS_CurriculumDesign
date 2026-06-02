cmake_minimum_required(VERSION 3.13)
project(UnixFileSystem)

set(CMAKE_CXX_STANDARD 17)
link_libraries(-lncurses)
include_directories(include)
add_executable(UnixFileSystem main.cpp include/superblock.h include/inode.h include/const.h include/image.h src/image.cpp src/filesystem.cpp include/filesystem.h include/dir.h src/editor.cpp include/editor.h)
