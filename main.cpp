#include <iostream>
#include <cstring>
#include <cstdio>
#include "image.h"
#include "filesystem.h"
#include "editor.h"

using namespace std;

int main() {
    system("clear");
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
    fclose(image.get_file_write());
    fclose(image.get_file_read());
    cout << "登出" << endl;
    return 0;
}