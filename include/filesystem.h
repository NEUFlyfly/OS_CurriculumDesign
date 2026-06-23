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
    friend class FileSystemAdapter;
private:
    FileSystemStorage storage;
    FileSystemState state;

    // Filesystem lifecycle and allocation
    int INodeAlloc();
    int BlockAlloc();
    bool FreeBlock(int i);
    bool INodeFree(int addr);
    void RepairBlockBitmapFromInodes();

    // Path navigation and directory metadata
    void FindDir(int inode_addr, const char name[]);
    void Chmod(int father_inode_addr, const char name[], int mode);
    void readFirst(int addr);
    void ShowDir(int addr);
    bool DeleteDir(int addr, char params[]);
    void DeleteFileOrDir(int addr);

    // File creation, data, deletion, and read commands
    bool Create(int father_inode_addr, const char name[], char file_content[]);
    bool MakeFile(int addr, char param[], char buffer[]);
    void DelFile(int addr, char param[]);
    void WriteFile(iNode inode, int inode_addr, char buffer[]);
    void Cat(int inode_addr, char name[]);

    // Login/session and user account commands
    bool check(char name[], char wd[]);
    void Quit();
    void AddUser(char username[]);
    void DeleteUser(char username[]);
    void DeleteUserContent(char buffer[], char *username);

    // Shell/editor adapters
    void editor(int cur_dir_addr, char file_name[], char buf[], int buf_size);
    void Help();
};

#endif //UNIXFILESYSTEM_FILESYSTEM_H
