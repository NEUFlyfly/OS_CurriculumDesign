#include "filesystem_adapter.h"
#include "filesystem.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#define _dup dup
#define _dup2 dup2
#define _close close
#define _fileno fileno
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
FileSystemAdapter::FileSystemAdapter(FileSystem& fs) : m_fs(fs) {}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------
bool FileSystemAdapter::Login(const std::string& username, const std::string& password) {
    // Reset to root directory - check() uses FindDir to navigate to /etc
    m_fs.state.cur_dir_addr = ROOT_DIR_ADDR;
    strcpy(m_fs.state.cur_dir_name, "/");
    // Also reset user name so FindDir permission checks work correctly
    memset(m_fs.state.cur_user_name, 0, sizeof(m_fs.state.cur_user_name));
    memset(m_fs.state.cur_grop_name, 0, sizeof(m_fs.state.cur_grop_name));

    char name[100] = {0};
    char passwd[100] = {0};
    strncpy(name, username.c_str(), sizeof(name) - 1);
    strncpy(passwd, password.c_str(), sizeof(passwd) - 1);

    std::cerr << "[Login] Attempting login for user: " << username << std::endl;
    std::cerr << "[Login] cur_dir_addr=" << m_fs.state.cur_dir_addr << " (ROOT=" << ROOT_DIR_ADDR << ")" << std::endl;

    bool success = false;
    captureCout([&]() {
        success = m_fs.check(name, passwd);
    });

    std::cerr << "[Login] check() returned: " << (success ? "true" : "false") << std::endl;

    if (success) {
        m_fs.is_login = true;
    }
    return success;
}

bool FileSystemAdapter::IsLoggedIn() const {
    return m_fs.is_login;
}

std::string FileSystemAdapter::GetCurrentUser() const {
    return std::string(m_fs.state.cur_user_name);
}

std::string FileSystemAdapter::GetCurrentPath() const {
    return std::string(m_fs.state.cur_dir_name);
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::ListDirectory() {
    // ShowDir() uses printf for most fields (tabs, links, owner, group, size, name)
    // and cout only for permission bits. We need fd-level redirection to capture both.
    return captureStdout([&]() {
        m_fs.ShowDir(m_fs.state.cur_dir_addr);
    });
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::ChangeDirectory(const std::string& path) {
    // Save old path so we can detect whether cd succeeded
    std::string oldPath = GetCurrentPath();

    // Handle absolute paths - go to root first
    if (!path.empty() && path[0] == '/') {
        // Navigate to root by going .. repeatedly
        while (GetCurrentPath() != "/") {
            char name[3] = "..";
            m_fs.FindDir(m_fs.state.cur_dir_addr, name);
        }
        // If path is just "/", we're done
        if (path == "/") {
            return "/";
        }
    }

    // Split path by "/" and navigate each component
    std::istringstream iss(path);
    std::string component;
    while (std::getline(iss, component, '/')) {
        if (component.empty() || component == ".") {
            continue;
        }

        std::string output = captureCout([&]() {
            char name[100] = {0};
            strncpy(name, component.c_str(), sizeof(name) - 1);
            m_fs.FindDir(m_fs.state.cur_dir_addr, name);
        });

        // If FindDir printed an error, return it
        if (!output.empty()) {
            return output;
        }
    }

    return GetCurrentPath();
}

// ---------------------------------------------------------------------------
// Directory operations
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::MakeDirectory(const std::string& name) {
    bool success = false;
    std::string output = captureCout([&]() {
        char dirname[100] = {0};
        strncpy(dirname, name.c_str(), sizeof(dirname) - 1);
        success = m_fs.MakeDir(m_fs.state.cur_dir_addr, dirname);
    });

    if (!output.empty()) return output;
    if (!success) return "Failed to create directory: " + name;
    return "Directory created: " + name;
}

std::string FileSystemAdapter::RemoveDirectory(const std::string& name) {
    bool success = false;
    std::string output = captureCout([&]() {
        char dirname[100] = {0};
        strncpy(dirname, name.c_str(), sizeof(dirname) - 1);
        success = m_fs.DeleteDir(m_fs.state.cur_dir_addr, dirname);
    });

    if (!output.empty()) return output;
    if (!success) return "Failed to remove directory: " + name;
    return "Directory removed: " + name;
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::CreateFile(const std::string& name) {
    std::string output = captureCout([&]() {
        char filename[100] = {0};
        char buffer[10000] = {0};
        strncpy(filename, name.c_str(), sizeof(filename) - 1);
        m_fs.MakeFile(m_fs.state.cur_dir_addr, filename, buffer);
    });

    if (!output.empty()) return output;
    return "File created: " + name;
}

std::string FileSystemAdapter::DeleteFile(const std::string& name) {
    std::string output = captureCout([&]() {
        char filename[100] = {0};
        strncpy(filename, name.c_str(), sizeof(filename) - 1);
        m_fs.DelFile(m_fs.state.cur_dir_addr, filename);
    });

    if (!output.empty()) return output;
    return "File deleted: " + name;
}

std::string FileSystemAdapter::ReadFile(const std::string& name) {
    // Cat() uses printf for file content and cout for error messages.
    // We need fd-level stdout redirection to capture both.
    return captureStdout([&]() {
        char filename[100] = {0};
        strncpy(filename, name.c_str(), sizeof(filename) - 1);
        m_fs.Cat(m_fs.state.cur_dir_addr, filename);
    });
}

std::string FileSystemAdapter::WriteFile(const std::string& name, const std::string& content) {
    // Find the file's inode in the current directory, then call FileSystem::WriteFile
    FILE* fr = m_fs.storage.image.get_file_read();
    Dir dir_vec[16];
    iNode cur, file_inode;

    fseek(fr, m_fs.state.cur_dir_addr, SEEK_SET);
    fread(&cur, sizeof(iNode), 1, fr);

    // Determine permission mode based on current user
    int filemode;
    if (strcmp(m_fs.state.cur_user_name, cur.user_name) == 0 || strcmp(m_fs.state.cur_user_name, "root") == 0)
        filemode = 6;
    else if (strcmp(m_fs.state.cur_grop_name, cur.user_group) == 0)
        filemode = 3;
    else
        filemode = 0;

    // Search for the file in the current directory
    size_t i = 0;
    int dir_id;
    bool found = false;
    int file_inode_addr = -1;

    while (i < 160) {
        dir_id = i / 16;
        if (cur.inode_dirblock[dir_id] == -1) {
            i += 16;
            continue;
        }
        fseek(fr, cur.inode_dirblock[dir_id], SEEK_SET);
        fread(dir_vec, sizeof(dir_vec), 1, fr);
        fflush(fr);

        for (int j = 0; j < 16; j++) {
            if (strcmp(dir_vec[j].name, name.c_str()) == 0) {
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                fread(&file_inode, sizeof(iNode), 1, fr);
                if (((file_inode.inode_mode >> 9) & 1) == 0) { // is a file
                    found = true;
                    file_inode_addr = dir_vec[j].inode_addr;
                }
                goto write_done_search;
            }
            i++;
        }
    }
write_done_search:

    if (!found) {
        return "File not found: " + name;
    }

    // Check write permission on the file
    if (((file_inode.inode_mode >> filemode >> 1) & 1) == 0) {
        return "Permission denied";
    }

    // Prepare buffer and write
    char buffer[FILE_BUFFER] = {0};
    strncpy(buffer, content.c_str(), sizeof(buffer) - 1);
    m_fs.WriteFile(file_inode, file_inode_addr, buffer);

    return ""; // empty string = success
}

// ---------------------------------------------------------------------------
// Permissions
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::ChangeMode(const std::string& name, int mode) {
    std::string output = captureCout([&]() {
        char filename[100] = {0};
        strncpy(filename, name.c_str(), sizeof(filename) - 1);
        m_fs.Chmod(m_fs.state.cur_dir_addr, filename, mode);
    });

    if (!output.empty()) return output;
    return "Permission changed for: " + name;
}

// ---------------------------------------------------------------------------
// User management
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::AddUser(const std::string& username) {
    std::string output = captureCout([&]() {
        char name[100] = {0};
        strncpy(name, username.c_str(), sizeof(name) - 1);
        m_fs.AddUser(name);
    });

    if (!output.empty()) return output;
    return "User added: " + username;
}

std::string FileSystemAdapter::DeleteUser(const std::string& username) {
    std::string output = captureCout([&]() {
        char name[100] = {0};
        strncpy(name, username.c_str(), sizeof(name) - 1);
        m_fs.DeleteUser(name);
    });

    if (!output.empty()) return output;
    return "User deleted: " + username;
}

// ---------------------------------------------------------------------------
// Disk usage
// ---------------------------------------------------------------------------
FileSystemAdapter::DiskUsage FileSystemAdapter::GetDiskUsage() const {
    DiskUsage usage;
    SuperBlock* sb = m_fs.storage.superBlock;
    usage.total_blocks  = sb->s_BLOCK_NUM;
    usage.free_blocks   = sb->s_free_BLOCK_NUM;
    usage.used_blocks   = usage.total_blocks - usage.free_blocks;
    usage.total_inodes  = sb->s_INODE_NUM;
    usage.free_inodes   = sb->s_free_INODE_NUM;
    return usage;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
std::string FileSystemAdapter::captureCout(std::function<void()> fn) {
    std::stringstream ss;
    std::streambuf* old = std::cout.rdbuf(ss.rdbuf());
    fn();
    std::cout.rdbuf(old);
    return ss.str();
}

std::string FileSystemAdapter::captureStdout(std::function<void()> fn) {
    char tmpname[L_tmpnam];
    std::tmpnam(tmpname);

    // Open a temporary file
    FILE* tmpf = fopen(tmpname, "w");
    if (!tmpf) return "Error: cannot create temp file for output capture";

    int tmpfd = _fileno(tmpf);

    // Save the original stdout fd (usually 1)
    fflush(stdout);
    int saved = _dup(1);

    // Redirect fd 1 to our temp file
    _dup2(tmpfd, 1);

    // Execute the function
    fn();

    // Flush everything still in C++ / C buffers
    std::cout.flush();
    fflush(stdout);

    // Restore fd 1
    _dup2(saved, 1);
    _close(saved);
    fclose(tmpf);

    // Reopen stdout as the console
    // (freopen is needed so the FILE* stdout points back to the real console)
#ifdef _WIN32
    freopen("CONOUT$", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    // Read captured content from temp file
    std::ifstream ifs(tmpname, std::ios::binary);
    std::string result((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    ifs.close();
    std::remove(tmpname);

    return result;
}
