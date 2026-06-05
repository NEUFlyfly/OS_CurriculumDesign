#include <iostream>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#endif
#include "image.h"
#include "filesystem.h"
#include "filesystem_adapter.h"

// Undef ncurses macros that conflict with ASIO
#undef timeout
#undef refresh
#undef clear
#undef move
#undef scroll
#undef erase

// Undef Windows macros that conflict with FileSystemAdapter method names
#undef RemoveDirectory
#undef CreateFile
#undef DeleteFile

#include "ws_server.h"

static WebSocketServer* g_server = nullptr;

static void ConfigureConsoleEncoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static void SignalHandler(int /*sig*/) {
    std::cout << "\nShutting down WebSocket server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main() {
    ConfigureConsoleEncoding();

    Image image;
    bool isFormat = false;
    image.InitImage(isFormat);

    auto* superBlock = new SuperBlock();

    FileSystem fileSystem(image, superBlock);
    if (isFormat) {
        std::cout << "Formating System...";
        if (!fileSystem.Format()) {
            std::cout << "Error" << std::endl;
        } else {
            std::cout << "Done" << std::endl;
        }
    }

    std::cout << "Installing FileSystem... ";
    if (!fileSystem.InitFileSystem()) {
        std::cout << "Error" << std::endl;
        exit(-1);
    }
    std::cout << "Done" << std::endl;

    FileSystemAdapter adapter(fileSystem);
    WebSocketServer server(adapter);
    g_server = &server;

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    server.start(9001);

    fclose(image.get_file_write());
    return 0;
}
