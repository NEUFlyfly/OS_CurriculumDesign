#include <iostream>
#include <cstring>
#include <cstdio>
#include "filesystem.h"

void FileSystem::ReadCommand(char *command) {
    char param_first[100];
    char param_second[100];
    char params_third[100];
    char buffer[10000];
    sscanf(command,"%s", param_first);
    if(strcmp(param_first, "ls")==0){
        ShowDir(state.cur_dir_addr);
    }
    else if(strcmp(param_first, "cd")==0){
        sscanf(command, "%s%s", param_first, param_second);
        FindDir(state.cur_dir_addr, param_second);
    }
    else if(strcmp(param_first,"mkdir")==0){
        sscanf(command, "%s%s", param_first, param_second);
        if(MakeDir(state.cur_dir_addr, param_second)) {
            Chmod(state.cur_dir_addr, param_second, 0660);
        }
    }
    else if(strcmp(param_first, "rmdir")==0){
        sscanf(command, "%s%s", param_first, param_second);
        DeleteDir(state.cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "nano")==0){	//创建一个文件
        char editor_buffer[FILE_BUFFER];
        sscanf(command, "%s%s", param_first, param_second);
        editor(state.cur_dir_addr, param_second, editor_buffer, FILE_BUFFER);	//读取内容到buf
    }
    else if(strcmp(param_first, "touch")==0){
        sscanf(command, "%s%s", param_first, param_second);
        (void)MakeFile(state.cur_dir_addr, param_second, buffer);	//读取内容到buf
    }
    else if(strcmp(param_first, "rm")==0){	//删除一个文件
        sscanf(command, "%s%s" , param_first, param_second);
        DelFile(state.cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "clear") == 0){
        system("cls");
    }
    else if(strcmp(param_first, "exit") == 0){
        Quit();
    }
    else if(strcmp(param_first, "useradd") == 0){
        param_second[0] = '\0';
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "Unknown option" << endl;
        }
        else{
            AddUser(param_second);
        }
    }
    else if(strcmp(param_first, "userdel") == 0){
        param_second[0] = '\0';
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "unlink user" << endl;
        }
        else{
            DeleteUser(param_second);
        }
    } else if(strcmp(param_first, "cat") == 0){
        sscanf(command, "%s%s", param_first, param_second);
        if(strlen(param_second) == 0){
            cout << "Unknown option" << endl;
        }
        Cat(state.cur_dir_addr, param_second);
    }
    else if(strcmp(param_first, "chmod")==0){
        param_second[0] = '\0';
        params_third[0] = '\0';
        sscanf(command, "%s%s%s", param_first, param_second, params_third);
        if(strlen(param_second)==0 || strlen(params_third)==0){
            cout << "usage: chmod [filename] [permissions] : Change the file permissions" << endl;
        }
        else{
            int num = 0;
            for(int i = 0; params_third[i]; i++) {
                num = num * 8 + params_third[i] - '0';
            }
            Chmod(state.cur_dir_addr, param_second, num);
        }
    }
    else if(strcmp(param_first, "help")==0){
        Help();
    }
    else if(strcmp(param_first, "format")==0){
        if(strcmp(state.cur_user_name, "root") != 0){
            cout << "Permission Dennied" << endl;
            return ;
        }
        Format();
        Quit();
    } else if(strcmp(param_first, "pwd")==0){
       cout << this->state.cur_dir_name << endl;
    }
    else{
        cout << "command not found: " << command << endl;
    }
    return ;
}

bool FileSystem::MakeMenu() {
    cout << "╭─" << state.cur_user_name << "@" << HOSTNAME << " ";
    cout << state.cur_dir_name << endl;
    cout << "╰─> ";
    return true;
}

void FileSystem::Help() {
    cout << "Command format : 'command [Necessary parameter] ([Unnecessary parameter])'" << endl;
    cout << "useradd [username] : Add user" << endl;
    cout << "userdel [username] : Delete users" << endl;
    cout << "exit : Exit the current user" << endl;
    cout << "cat [filename] : Output file content" << endl;
    cout << "rm [filename] : Remove file" << endl;
    cout << "mkdir [directoryName] : Create subdirectory" << endl;
    cout << "rmdir [directoryName] : Delete subdirectory" << endl;
    cout << "cd [directoryName] : Change current directory" << endl;
    cout << "ls : List the file directory" << endl;
    cout << "pwd : List the current directory name" << endl;
    cout << "chmod [filename] [permissions] : Change the file permissions" << endl;
    cout << "clear : Clear the terminal" << endl;
    cout << "exit : Exit the System" << endl;
    cout << "touch [filename] : Create a new empty file" << endl;
    cout << "nano [filename] : edit a file" << endl;
    cout << "format : format the system" << endl;
}
