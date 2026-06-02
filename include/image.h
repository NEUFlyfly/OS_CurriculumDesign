//
// Created by 林智超 on 2019-06-17.
//

#ifndef UNIXFILESYSTEM_IMAGE_H
#define UNIXFILESYSTEM_IMAGE_H

#include <string>
#include <fstream>
#include "const.h"


using namespace std;

class Image {
public:
    Image();

    bool InitImage(bool& isFormat);
    const string &get_filename() const {
        return _filename;
    }

    void set_filename(const string &_filename) {
        Image::_filename = _filename;
    }

    FILE *get_file_read() const {
        return _file_read;
    }

    void set_file_read(FILE *_file_read) {
        Image::_file_read = _file_read;
    }

    FILE *get_file_write() const {
        return _file_write;
    }

    void set_file_write(FILE *_file_write) {
        Image::_file_write = _file_write;
    }

private:
    string _filename;
    FILE* _file_read;
    FILE* _file_write;
};




#endif //UNIXFILESYSTEM_IMAGE_H
