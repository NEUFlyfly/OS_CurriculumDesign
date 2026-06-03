//
// Created by 林智超 on 2019-06-17.
//

#ifndef UNIXFILESYSTEM_FILESYSTEM_H
#define UNIXFILESYSTEM_FILESYSTEM_H

#include "image.h"
#include "inode.h"
#include "superblock.h"
#include "dir.h"
#include "editor.h"
#include "filesystem_storage.h"
#include "filesystem_state.h"


class FileSystem {
public:
    FileSystem(Image& _image, SuperBlock* _superBlock);
    bool InitFileSystem();
    bool Format();
    bool MakeMenu();
    bool Login();
    bool MakeDir(int inode_addr, char name[]);
    void ReadCommand(char string[]);

    bool is_login;
private:
    FileSystemStorage storage;
    FileSystemState state;

    void readFirst(int addr);


    bool check(char name[], char wd[]);

    void FindDir(int inode_addr, const char name[]);

    int INodeAlloc();

    int BlockAlloc();

    bool Create(int father_inode_addr, const char name[], char file_content[]);

    void Chmod(int father_inode_addr, const char name[], int mode);


    void ShowDir(int addr);

    bool DeleteDir(int addr, char params[]);

    void DeleteFileOrDir(int addr);

    bool FreeBlock(int i);

    bool INodeFree(int addr);

    void Quit();

    void MakeFile(int addr, char param[], char buffer[]);

    void DelFile(int addr, char param[]);

    void AddUser(char username[]);

    void DeleteUser(char username[]);

    void WriteFile(iNode inode, int inode_addr, char buffer[]);

    void DeleteUserContent(char buffer[], char *username);

    void Cat(int inode_addr, char name[]);

    void editor(int cur_dir_addr, char file_name[], char buf[]);

    void Help();
};

#endif //UNIXFILESYSTEM_FILESYSTEM_H
