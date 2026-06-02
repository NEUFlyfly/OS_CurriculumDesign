//
// Created by 林智超 on 2019-06-17.
//

#include <iostream>
#include <image.h>

#include "const.h"
#include "image.h"

Image::Image() {
    this->_filename = IMAGEFILENAME;
    this->_file_read = nullptr;
    this->_file_write = nullptr;
}




bool Image::InitImage(bool &isFormat) {
    cout << "Init image file ...";
    FILE *image_file = fopen(IMAGEFILENAME, "rb");
    if(image_file == nullptr) {
        //不存在重新创建一个
        FILE *created_file = fopen(IMAGEFILENAME, "wb");
        if (created_file == nullptr) {
            cout << "Error" << endl;
            exit(-1);
        }
        fclose(created_file);
        isFormat = true;
    } else {
        fclose(image_file);
    }

    this->_file_write = fopen(IMAGEFILENAME,"rb+");
    this->_file_read = this->_file_write;
    if(this->_file_write == nullptr){
        printf("虚拟磁盘文件打开失败\n");
        return false;	//打开文件失败
    }

    cout << "Done" << endl;
    return !isFormat;
}


