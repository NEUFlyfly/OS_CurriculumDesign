#include <iostream>
#include <cstring>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
#include "image.h"
#include "filesystem.h"
#include "editor.h"

using namespace std;

static void ConfigureConsoleEncoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int main() {
    ConfigureConsoleEncoding();
    system("cls");
    Image image;
    bool isFormat = false;
    image.InitImage(isFormat);

    auto * superBlock = new SuperBlock();

    FileSystem fileSystem(image, superBlock);
    if(isFormat) {
        cout << "Formating System...";
        if(!fileSystem.Format()) {
            cout << "Error" << endl;
        } else {
            cout << "Done" << endl;
        }
    }

    cout << "Installing FileSystem... ";
    if(!fileSystem.InitFileSystem()) {
        cout << "Error" << endl;
        exit(-1);
    } else {
        cout << "Done" << endl;
    }
    while(!fileSystem.Login());
    while(fileSystem.is_login) {
        char str[100];
        if (!fileSystem.MakeMenu()) {
            cout << "Error" << endl;
            break;
        }
        cin.getline(str, 100);
        fileSystem.ReadCommand(str);
    }
    fclose(image.get_file_write());  // _file_read 指向同一句柄，无需重复关闭
    cout << "登出" << endl;
    return 0;
}
