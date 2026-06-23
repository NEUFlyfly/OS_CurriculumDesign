#include "filesystem_adapter.h"
#include "filesystem.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <iomanip>
#include <utility>

namespace {
std::string permissionsFromMode(unsigned int mode) {
    std::string permissions(9, '-');
    const char labels[3] = {'r', 'w', 'x'};

    for (int group = 0; group < 3; ++group) {
        for (int bit = 0; bit < 3; ++bit) {
            const int modeBit = 8 - (group * 3 + bit);
            if (((mode >> modeBit) & 1U) == 1U) {
                permissions[group * 3 + bit] = labels[bit];
            }
        }
    }

    return permissions;
}

std::string permissionOctalFromMode(unsigned int mode) {
    std::ostringstream oss;
    oss << '0' << std::oct << std::setfill('0') << std::setw(3) << (mode & 0777U);
    return oss.str();
}

bool isValidDataBlockAddress(int address) {
    const int dataBlockStart = static_cast<int>(BLOCK_STARTADDR);
    const int blockSize = static_cast<int>(BLOCK_SIZE);
    const int totalBlocks = static_cast<int>(BLOCK_NUM);
    return address >= dataBlockStart
        && (address - dataBlockStart) % blockSize == 0
        && (address - dataBlockStart) / blockSize < totalBlocks;
}

unsigned int toDataBlockIndex(int address) {
    const int dataBlockStart = static_cast<int>(BLOCK_STARTADDR);
    const int blockSize = static_cast<int>(BLOCK_SIZE);
    return static_cast<unsigned int>((address - dataBlockStart) / blockSize);
}

int toDataBlockIndexOrMinusOne(int address) {
    return isValidDataBlockAddress(address) ? static_cast<int>(toDataBlockIndex(address)) : -1;
}

bool containsAddress(const std::vector<int>& values, int address) {
    for (std::vector<int>::const_iterator it = values.begin(); it != values.end(); ++it) {
        if (*it == address) return true;
    }
    return false;
}

std::string boundedCString(const char* value, std::size_t size) {
    std::size_t length = 0;
    while (length < size && value[length] != '\0') {
        ++length;
    }
    return std::string(value, length);
}

unsigned int offsetToBlock(unsigned int offset) {
    return offset / BLOCK_SIZE;
}

unsigned int regionBlockCount(unsigned int startOffset, unsigned int endOffset) {
    return (endOffset - startOffset) / BLOCK_SIZE;
}

FileSystemAdapter::DiskRegionSnapshot makeDiskRegion(
    const std::string& id,
    const std::string& label,
    const std::string& kind,
    const std::string& state,
    const std::string& description,
    unsigned int startOffset,
    unsigned int blockCount
) {
    FileSystemAdapter::DiskRegionSnapshot region;
    region.id = id;
    region.label = label;
    region.kind = kind;
    region.state = state;
    region.description = description;
    region.startBlock = offsetToBlock(startOffset);
    region.blockCount = blockCount;
    region.startByte = startOffset;
    region.endByte = blockCount == 0 ? startOffset : startOffset + blockCount * BLOCK_SIZE - 1;
    return region;
}

FileSystemAdapter::DiskBlockSnapshot makeDiskBlock(
    unsigned int index,
    const FileSystemAdapter::DiskRegionSnapshot& region,
    const std::string& state
) {
    FileSystemAdapter::DiskBlockSnapshot block;
    block.index = index;
    block.byteOffset = index * BLOCK_SIZE;
    block.region = region.id;
    block.regionLabel = region.label;
    block.kind = region.kind;
    block.state = state;
    block.dataBlockIndex = -1;
    block.dataBlockAddress = -1;
    block.usedRecords = 0;
    block.totalRecords = 0;
    return block;
}

const std::size_t MAX_TEXT_FILE_BYTES = BLOCK_SIZE * 10;

bool isDirectoryInode(const iNode& inode) {
    return ((inode.inode_mode >> 9) & 1) == 1;
}

std::string normalizePath(const std::string& basePath, const std::string& path) {
    std::string combined;
    if (path.empty()) {
        combined = basePath.empty() ? "/" : basePath;
    } else if (path[0] == '/') {
        combined = path;
    } else {
        combined = (basePath.empty() || basePath == "/") ? "/" + path : basePath + "/" + path;
    }

    std::vector<std::string> parts;
    std::istringstream iss(combined);
    std::string part;
    while (std::getline(iss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
            continue;
        }
        parts.push_back(part);
    }

    if (parts.empty()) return "/";
    std::string normalized;
    for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it) {
        normalized += "/" + *it;
    }
    return normalized;
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string part;
    while (std::getline(iss, part, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    return parts;
}

bool readInodeAt(FILE* fr, int address, iNode& inode) {
    if (address < 0) return false;
    fseek(fr, address, SEEK_SET);
    return fread(&inode, sizeof(iNode), 1, fr) == 1;
}

bool findChild(FILE* fr, int directoryAddress, const std::string& name, int& childAddress, iNode& childInode) {
    iNode directory;
    if (!readInodeAt(fr, directoryAddress, directory) || !isDirectoryInode(directory)) return false;

    for (int blockIndex = 0; blockIndex < 10; ++blockIndex) {
        if (directory.inode_dirblock[blockIndex] == -1) continue;

        Dir entries[16] = {0};
        fseek(fr, directory.inode_dirblock[blockIndex], SEEK_SET);
        if (fread(&entries, sizeof(entries), 1, fr) != 1) return false;

        for (int entryIndex = 0; entryIndex < 16; ++entryIndex) {
            if (std::strcmp(entries[entryIndex].name, "") == 0) continue;
            if (name == entries[entryIndex].name) {
                childAddress = entries[entryIndex].inode_addr;
                return readInodeAt(fr, childAddress, childInode);
            }
        }
    }

    return false;
}

struct ResolvedEntry {
    bool found;
    std::string error;
    std::string path;
    std::string name;
    int parentAddress;
    int inodeAddress;
    iNode inode;
};

ResolvedEntry resolveEntry(FILE* fr, const std::string& path) {
    ResolvedEntry resolved;
    resolved.found = false;
    resolved.path = path;
    resolved.name = path == "/" ? "/" : "";
    resolved.parentAddress = -1;
    resolved.inodeAddress = -1;
    std::memset(&resolved.inode, 0, sizeof(resolved.inode));

    if (path == "/") {
        if (readInodeAt(fr, ROOT_DIR_ADDR, resolved.inode)) {
            resolved.found = true;
            resolved.inodeAddress = ROOT_DIR_ADDR;
        } else {
            resolved.error = "Failed to read root directory";
        }
        return resolved;
    }

    std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) {
        resolved.error = "Invalid path";
        return resolved;
    }

    int currentAddress = ROOT_DIR_ADDR;
    iNode currentInode;
    if (!readInodeAt(fr, currentAddress, currentInode)) {
        resolved.error = "Failed to read root directory";
        return resolved;
    }

    for (std::size_t i = 0; i < parts.size(); ++i) {
        int childAddress = -1;
        iNode childInode;
        if (!findChild(fr, currentAddress, parts[i], childAddress, childInode)) {
            resolved.error = "Path not found: " + path;
            return resolved;
        }

        if (i == parts.size() - 1) {
            resolved.found = true;
            resolved.name = parts[i];
            resolved.parentAddress = currentAddress;
            resolved.inodeAddress = childAddress;
            resolved.inode = childInode;
            return resolved;
        }

        if (!isDirectoryInode(childInode)) {
            resolved.error = "Not a directory: " + parts[i];
            return resolved;
        }

        currentAddress = childAddress;
        currentInode = childInode;
    }

    resolved.error = "Invalid path";
    return resolved;
}

ResolvedEntry resolveParentDirectory(FILE* fr, const std::string& path, std::string& entryName) {
    ResolvedEntry resolved;
    resolved.found = false;
    resolved.parentAddress = -1;
    resolved.inodeAddress = -1;
    std::memset(&resolved.inode, 0, sizeof(resolved.inode));

    std::vector<std::string> parts = splitPath(path);
    if (parts.empty()) {
        resolved.error = "Cannot operate on root directory";
        return resolved;
    }

    entryName = parts.back();
    parts.pop_back();
    std::string parentPath = "/";
    for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it) {
        parentPath += (parentPath == "/" ? "" : "/") + *it;
    }

    resolved = resolveEntry(fr, parentPath);
    if (resolved.found && !isDirectoryInode(resolved.inode)) {
        resolved.found = false;
        resolved.error = "Target parent is not a directory";
    }
    return resolved;
}

bool pathIsDescendant(const std::string& parent, const std::string& candidate) {
    return parent != "/" && candidate.size() > parent.size()
        && candidate.compare(0, parent.size(), parent) == 0
        && candidate[parent.size()] == '/';
}

std::vector<std::pair<std::string, int> > listDirectoryChildren(FILE* fr, const iNode& directory) {
    std::vector<std::pair<std::string, int> > children;
    for (int blockIndex = 0; blockIndex < 10; ++blockIndex) {
        if (directory.inode_dirblock[blockIndex] == -1) continue;
        Dir entries[16] = {0};
        fseek(fr, directory.inode_dirblock[blockIndex], SEEK_SET);
        if (fread(&entries, sizeof(entries), 1, fr) != 1) break;
        for (int entryIndex = 0; entryIndex < 16; ++entryIndex) {
            const std::string name(entries[entryIndex].name);
            if (name.empty() || name == "." || name == "..") continue;
            children.push_back(std::make_pair(name, entries[entryIndex].inode_addr));
        }
    }
    return children;
}

std::string readFileContent(FILE* fr, const iNode& inode) {
    if (isDirectoryInode(inode)) return "";
    if (inode.inode_size > MAX_TEXT_FILE_BYTES) return "";

    std::string content;
    unsigned int remaining = inode.inode_size;
    for (int blockIndex = 0; blockIndex < 10 && remaining > 0; ++blockIndex) {
        if (inode.inode_dirblock[blockIndex] == -1) return "";
        char buffer[BLOCK_SIZE] = {0};
        fseek(fr, inode.inode_dirblock[blockIndex], SEEK_SET);
        if (fread(buffer, 1, BLOCK_SIZE, fr) == 0 && remaining > 0) return "";
        const unsigned int chunkSize = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        content.append(buffer, buffer + chunkSize);
        remaining -= chunkSize;
    }
    return content;
}

bool isSupportedTextContent(const std::string& content) {
    return content.size() <= MAX_TEXT_FILE_BYTES && content.find('\0') == std::string::npos;
}
}

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
    if (username.empty() || password.empty()) {
        m_fs.is_login = false;
        return false;
    }

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

std::string FileSystemAdapter::Help() {
    return captureCout([&]() {
        m_fs.Help();
    });
}

std::string FileSystemAdapter::Logout() {
    return captureCout([&]() {
        m_fs.Quit();
    });
}

std::string FileSystemAdapter::FormatAndLogout() {
    std::string output = captureCout([&]() {
        m_fs.Format();
        m_fs.Quit();
    });

    if (!output.empty()) return output;
    return "File system formatted";
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

FileSystemAdapter::FileProperties FileSystemAdapter::GetEntryProperties(const std::string& name) const {
    FileProperties properties;
    properties.found = false;
    properties.error = "File not found: " + name;
    properties.name = name;
    properties.type = "";
    properties.location = GetCurrentPath();
    properties.size = 0;
    properties.permissions = "";
    properties.permissionOctal = "";
    properties.owner = "";
    properties.group = "";
    properties.inodeAddress = -1;
    properties.inodeId = 0;
    properties.linkCount = 0;
    properties.modified = "";

    if (name.empty()) {
        properties.error = "Missing file name";
        return properties;
    }

    FILE* fr = m_fs.storage.image.get_file_read();
    iNode cur;
    fseek(fr, m_fs.state.cur_dir_addr, SEEK_SET);
    if (fread(&cur, sizeof(iNode), 1, fr) != 1) {
        properties.error = "Failed to read current directory";
        return properties;
    }

    int visited = 0;
    while (visited < cur.inode_cnt && visited < 160) {
        const int dirBlockIndex = visited / 16;
        if (cur.inode_dirblock[dirBlockIndex] == -1) {
            visited += 16;
            continue;
        }

        Dir dir_vec[16] = {0};
        fseek(fr, cur.inode_dirblock[dirBlockIndex], SEEK_SET);
        if (fread(&dir_vec, sizeof(dir_vec), 1, fr) != 1) {
            properties.error = "Failed to read directory block";
            return properties;
        }

        for (int j = 0; j < 16 && visited < cur.inode_cnt; ++j) {
            if (std::strcmp(dir_vec[j].name, "") != 0 && name == dir_vec[j].name) {
                iNode entry;
                fseek(fr, dir_vec[j].inode_addr, SEEK_SET);
                if (fread(&entry, sizeof(iNode), 1, fr) != 1) {
                    properties.error = "Failed to read inode: " + name;
                    return properties;
                }

                properties.found = true;
                properties.error = "";
                properties.type = ((entry.inode_mode >> 9) & 1U) == 1U ? "dir" : "file";
                properties.size = entry.inode_size;
                properties.permissions = permissionsFromMode(entry.inode_mode);
                properties.permissionOctal = permissionOctalFromMode(entry.inode_mode);
                properties.owner = boundedCString(entry.user_name, sizeof(entry.user_name));
                properties.group = boundedCString(entry.user_group, sizeof(entry.user_group));
                properties.inodeAddress = dir_vec[j].inode_addr;
                properties.inodeId = entry.inode_id;
                properties.linkCount = entry.inode_cnt;
                return properties;
            }
            ++visited;
        }
    }

    return properties;
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
    bool success = false;
    std::string output = captureCout([&]() {
        char filename[100] = {0};
        char buffer[10000] = {0};
        strncpy(filename, name.c_str(), sizeof(filename) - 1);
        success = m_fs.MakeFile(m_fs.state.cur_dir_addr, filename, buffer);
    });

    if (!success) return output.empty() ? "Failed to create file: " + name : output;
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

std::string FileSystemAdapter::CopyEntry(const std::string& sourcePath, const std::string& targetPath) {
    const std::string normalizedSource = normalizePath(GetCurrentPath(), sourcePath);
    const std::string normalizedTarget = normalizePath(GetCurrentPath(), targetPath);

    if (normalizedSource == "/") {
        return "Cannot copy root directory";
    }

    FILE* fr = m_fs.storage.image.get_file_read();
    ResolvedEntry source = resolveEntry(fr, normalizedSource);
    if (!source.found) return source.error;

    ResolvedEntry target = resolveEntry(fr, normalizedTarget);
    if (!target.found) return target.error;
    if (!isDirectoryInode(target.inode)) return "Paste target is not a directory";

    if (isDirectoryInode(source.inode)
        && (normalizedSource == normalizedTarget || pathIsDescendant(normalizedSource, normalizedTarget))) {
        return "Cannot copy a directory into itself";
    }

    std::function<std::string(int, int, const std::string&)> copyRecursive;
    copyRecursive = [&](int sourceAddress, int targetDirectoryAddress, const std::string& targetName) -> std::string {
        if (targetName.empty() || targetName.find('/') != std::string::npos || targetName.find('\\') != std::string::npos) {
            return "Invalid file name: " + targetName;
        }
        if (targetName.size() >= MAX_NAME_SIZE) {
            return "File name is too long: " + targetName;
        }

        
        int existingAddress = -1;
        iNode existingInode;
        if (findChild(fr, targetDirectoryAddress, targetName, existingAddress, existingInode)) {
            return "Destination already exists: " + targetName;
        }

        iNode sourceInode;
        if (!readInodeAt(fr, sourceAddress, sourceInode)) {
            return "Failed to read source inode";
        }

        if (isDirectoryInode(sourceInode)) {
            bool created = false;
            std::string output = captureCout([&]() {
                char dirname[100] = {0};
                std::strncpy(dirname, targetName.c_str(), sizeof(dirname) - 1);
                created = m_fs.MakeDir(targetDirectoryAddress, dirname);
            });
            if (!output.empty()) return output;
            if (!created) return "Failed to create directory: " + targetName;

            int newDirectoryAddress = -1;
            iNode newDirectoryInode;
            if (!findChild(fr, targetDirectoryAddress, targetName, newDirectoryAddress, newDirectoryInode)) {
                return "Failed to locate copied directory: " + targetName;
            }

            std::vector<std::pair<std::string, int> > children = listDirectoryChildren(fr, sourceInode);
            for (std::vector<std::pair<std::string, int> >::const_iterator it = children.begin(); it != children.end(); ++it) {
                std::string childResult = copyRecursive(it->second, newDirectoryAddress, it->first);
                if (!childResult.empty()) return childResult;
            }
            return "";
        }

        if (sourceInode.inode_size > MAX_TEXT_FILE_BYTES) {
            std::ostringstream oss;
            oss << "File too large: maximum supported size is " << MAX_TEXT_FILE_BYTES << " bytes";
            return oss.str();
        }

        std::string content = readFileContent(fr, sourceInode);
        if (content.size() != sourceInode.inode_size) {
            return "Failed to read source file content";
        }
        if (!isSupportedTextContent(content)) {
            return "Only text files up to 5120 bytes are supported";
        }

        bool created = false;
        std::string output = captureCout([&]() {
            char filename[100] = {0};
            char buffer[FILE_BUFFER] = {0};
            std::strncpy(filename, targetName.c_str(), sizeof(filename) - 1);
            if (!content.empty()) std::memcpy(buffer, content.data(), content.size());
            created = m_fs.Create(targetDirectoryAddress, filename, buffer);
        });
        if (!output.empty()) return output;
        if (!created) return "Failed to create file: " + targetName;
        return "";
    };

    std::string copyResult = copyRecursive(source.inodeAddress, target.inodeAddress, source.name);
    if (!copyResult.empty()) return copyResult;
    return "Copied: " + normalizedSource;
}

std::string FileSystemAdapter::MoveEntry(const std::string& sourcePath, const std::string& targetPath) {
    const std::string normalizedSource = normalizePath(GetCurrentPath(), sourcePath);
    const std::string normalizedTarget = normalizePath(GetCurrentPath(), targetPath);

    if (normalizedSource == "/") {
        return "Cannot move root directory";
    }
    if (normalizedSource == normalizedTarget || pathIsDescendant(normalizedSource, normalizedTarget)) {
        return "Cannot move a directory into itself";
    }

    FILE* fr = m_fs.storage.image.get_file_read();
    ResolvedEntry source = resolveEntry(fr, normalizedSource);
    if (!source.found) return source.error;

    std::string copyResult = CopyEntry(normalizedSource, normalizedTarget);
    if (copyResult.find("Copied:") != 0) return copyResult;

    std::string sourceName;
    ResolvedEntry parent = resolveParentDirectory(fr, normalizedSource, sourceName);
    if (!parent.found) return parent.error;

    if (isDirectoryInode(source.inode)) {
        bool removed = false;
        std::string output = captureCout([&]() {
            char dirname[100] = {0};
            std::strncpy(dirname, sourceName.c_str(), sizeof(dirname) - 1);
            removed = m_fs.DeleteDir(parent.inodeAddress, dirname);
        });
        if (!output.empty()) return output;
        if (!removed) return "Failed to remove source directory after move";
    } else {
        std::string output = captureCout([&]() {
            char filename[100] = {0};
            std::strncpy(filename, sourceName.c_str(), sizeof(filename) - 1);
            m_fs.DelFile(parent.inodeAddress, filename);
        });
        if (!output.empty()) return output;
    }

    return "Moved: " + normalizedSource;
}

std::string FileSystemAdapter::UploadFile(const std::string& targetPath, const std::string& name, const std::string& content) {
    const std::string normalizedTarget = normalizePath(GetCurrentPath(), targetPath);
    if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        return "Invalid file name";
    }
    if (name.size() >= MAX_NAME_SIZE) {
        return "File name is too long: " + name;
    }
    if (!isSupportedTextContent(content)) {
        std::ostringstream oss;
        oss << "Only text files up to " << MAX_TEXT_FILE_BYTES << " bytes are supported";
        return oss.str();
    }

    FILE* fr = m_fs.storage.image.get_file_read();
    ResolvedEntry target = resolveEntry(fr, normalizedTarget);
    if (!target.found) return target.error;
    if (!isDirectoryInode(target.inode)) return "Upload target is not a directory";

    int existingAddress = -1;
    iNode existingInode;
    if (findChild(fr, target.inodeAddress, name, existingAddress, existingInode)) {
        return "Destination already exists: " + name;
    }

    bool created = false;
    std::string output = captureCout([&]() {
        char filename[100] = {0};
        char buffer[FILE_BUFFER] = {0};
        std::strncpy(filename, name.c_str(), sizeof(filename) - 1);
        if (!content.empty()) std::memcpy(buffer, content.data(), content.size());
        created = m_fs.Create(target.inodeAddress, filename, buffer);
    });
    if (!output.empty()) return output;
    if (!created) return "Failed to upload file: " + name;
    return "Uploaded: " + name;
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

std::string FileSystemAdapter::AddUser(const std::string& username, const std::string& password) {
    if (username.empty()) {
        return "username is required";
    }
    if (password.empty()) {
        return "password is required";
    }

    std::istringstream input(password + "\n");
    std::streambuf* originalCin = std::cin.rdbuf(input.rdbuf());

    std::string output;
    try {
        output = captureCout([&]() {
            char name[100] = {0};
            strncpy(name, username.c_str(), sizeof(name) - 1);
            m_fs.AddUser(name);
        });
    } catch (...) {
        std::cin.rdbuf(originalCin);
        throw;
    }

    std::cin.rdbuf(originalCin);

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
// Storage inspection
// ---------------------------------------------------------------------------
FileSystemAdapter::StorageInspectionSnapshot FileSystemAdapter::GetStorageInspectionSnapshot() const {
    StorageInspectionSnapshot snapshot;
    SuperBlock* sb = m_fs.storage.superBlock;

    snapshot.layout.blockSize = BLOCK_SIZE;
    snapshot.layout.inodeSize = INODE_SIZE;
    snapshot.layout.totalBlocks = sb->s_BLOCK_NUM;
    snapshot.layout.totalInodes = sb->s_INODE_NUM;
    snapshot.layout.superblockStart = SUPERBLOCK_START_ADDR;
    snapshot.layout.inodeBitmapStart = INODEBITMAP_START_ADDR;
    snapshot.layout.blockBitmapStart = BLOCKBIITMAP_START_ADDR;
    snapshot.layout.inodeTableStart = INODE_START_ADDR;
    snapshot.layout.dataBlockStart = BLOCK_STARTADDR;

    snapshot.summary.freeBlocks = sb->s_free_BLOCK_NUM;
    snapshot.summary.usedBlocks = snapshot.layout.totalBlocks - snapshot.summary.freeBlocks;
    snapshot.summary.freeInodes = sb->s_free_INODE_NUM;
    snapshot.summary.usedInodes = snapshot.layout.totalInodes - snapshot.summary.freeInodes;

    FILE* fr = m_fs.storage.image.get_file_read();

    snapshot.groupedLinking.blocksPerGroup = sb->s_blocks_per_group;
    snapshot.groupedLinking.freeBlockCount = sb->s_free_BLOCK_NUM;
    snapshot.groupedLinking.currentTop = sb->s_free_BLOCK_NUM == 0 || sb->s_blocks_per_group == 0
        ? -1
        : static_cast<int>((sb->s_free_BLOCK_NUM - 1) % sb->s_blocks_per_group);
    snapshot.groupedLinking.currentGroupBlockAddress = sb->s_free_addr;
    snapshot.groupedLinking.currentGroupBlockIndex = toDataBlockIndexOrMinusOne(sb->s_free_addr);
    snapshot.groupedLinking.truncated = false;
    snapshot.groupedLinking.superBlockStack.reserve(sb->s_blocks_per_group);
    for (unsigned int i = 0; i < sb->s_blocks_per_group; ++i) {
        snapshot.groupedLinking.superBlockStack.push_back(sb->s_free[i]);
    }

    GroupedLinkingGroup currentGroup;
    currentGroup.order = 0;
    currentGroup.source = "SuperBlock";
    currentGroup.groupBlockAddress = sb->s_free_addr;
    currentGroup.groupBlockIndex = toDataBlockIndexOrMinusOne(sb->s_free_addr);
    currentGroup.nextGroupAddress = sb->s_blocks_per_group > 0 ? sb->s_free[0] : -1;
    currentGroup.nextGroupIndex = toDataBlockIndexOrMinusOne(currentGroup.nextGroupAddress);
    currentGroup.top = snapshot.groupedLinking.currentTop;
    currentGroup.stack = snapshot.groupedLinking.superBlockStack;
    if (currentGroup.top >= 0) {
        if (isValidDataBlockAddress(currentGroup.groupBlockAddress)) {
            currentGroup.freeBlockAddresses.push_back(currentGroup.groupBlockAddress);
            currentGroup.freeBlockIndexes.push_back(toDataBlockIndex(currentGroup.groupBlockAddress));
        }
        for (int i = 1; i <= currentGroup.top && i < static_cast<int>(sb->s_blocks_per_group); ++i) {
            const int address = sb->s_free[i];
            if (isValidDataBlockAddress(address)) {
                currentGroup.freeBlockAddresses.push_back(address);
                currentGroup.freeBlockIndexes.push_back(toDataBlockIndex(address));
            }
        }
    }
    snapshot.groupedLinking.groups.push_back(currentGroup);

    std::vector<int> visitedGroupBlocks;
    visitedGroupBlocks.push_back(sb->s_free_addr);
    int nextGroupAddress = currentGroup.nextGroupAddress;
    unsigned int order = 1;
    const unsigned int maxGroups = BLOCK_NUM / BLOCKS_PER_GROUP + 1;
    while (isValidDataBlockAddress(nextGroupAddress) && order < maxGroups) {
        if (containsAddress(visitedGroupBlocks, nextGroupAddress)) {
            snapshot.groupedLinking.truncated = true;
            break;
        }

        int stack[BLOCKS_PER_GROUP] = {0};
        fseek(fr, nextGroupAddress, SEEK_SET);
        const std::size_t readCount = fread(stack, sizeof(stack), 1, fr);
        if (readCount != 1) {
            snapshot.groupedLinking.truncated = true;
            break;
        }

        GroupedLinkingGroup group;
        group.order = order;
        group.source = "DataBlock";
        group.groupBlockAddress = nextGroupAddress;
        group.groupBlockIndex = toDataBlockIndexOrMinusOne(nextGroupAddress);
        group.nextGroupAddress = stack[0];
        group.nextGroupIndex = toDataBlockIndexOrMinusOne(group.nextGroupAddress);
        group.top = static_cast<int>(sb->s_blocks_per_group) - 1;
        group.stack.reserve(sb->s_blocks_per_group);
        for (unsigned int i = 0; i < sb->s_blocks_per_group; ++i) {
            group.stack.push_back(stack[i]);
        }
        if (isValidDataBlockAddress(group.groupBlockAddress)) {
            group.freeBlockAddresses.push_back(group.groupBlockAddress);
            group.freeBlockIndexes.push_back(toDataBlockIndex(group.groupBlockAddress));
        }
        for (unsigned int i = 1; i < sb->s_blocks_per_group; ++i) {
            const int address = stack[i];
            if (isValidDataBlockAddress(address)) {
                group.freeBlockAddresses.push_back(address);
                group.freeBlockIndexes.push_back(toDataBlockIndex(address));
            }
        }

        visitedGroupBlocks.push_back(nextGroupAddress);
        nextGroupAddress = group.nextGroupAddress;
        snapshot.groupedLinking.groups.push_back(group);
        ++order;
    }

    if (isValidDataBlockAddress(nextGroupAddress)) {
        snapshot.groupedLinking.truncated = true;
    }

    snapshot.blockBitmap.reserve(snapshot.layout.totalBlocks);
    for (unsigned int i = 0; i < snapshot.layout.totalBlocks; ++i) {
        snapshot.blockBitmap.push_back(m_fs.storage.block_bitmap[i] ? 1 : 0);
    }

    snapshot.inodeBitmap.reserve(snapshot.layout.totalInodes);
    for (unsigned int i = 0; i < snapshot.layout.totalInodes; ++i) {
        snapshot.inodeBitmap.push_back(m_fs.storage.inode_bitmap[i] ? 1 : 0);
    }

    const unsigned int superblockBlocks = regionBlockCount(SUPERBLOCK_START_ADDR, INODEBITMAP_START_ADDR);
    const unsigned int inodeBitmapBlocks = regionBlockCount(INODEBITMAP_START_ADDR, BLOCKBIITMAP_START_ADDR);
    const unsigned int blockBitmapBlocks = regionBlockCount(BLOCKBIITMAP_START_ADDR, INODE_START_ADDR);
    const unsigned int inodeTableBlocks = regionBlockCount(INODE_START_ADDR, BLOCK_STARTADDR);
    const unsigned int dataStartBlock = offsetToBlock(BLOCK_STARTADDR);

    DiskRegionSnapshot superblockRegion = makeDiskRegion(
        "superblock",
        "SuperBlock",
        "metadata",
        "reserved",
        "保存文件系统总体容量、空闲块栈和各区域起始地址。",
        SUPERBLOCK_START_ADDR,
        superblockBlocks
    );
    DiskRegionSnapshot inodeBitmapRegion = makeDiskRegion(
        "inode_bitmap",
        "Inode 位图",
        "metadata",
        "metadata-used",
        "记录每个 inode 是否被分配，本区域自身属于元数据块。",
        INODEBITMAP_START_ADDR,
        inodeBitmapBlocks
    );
    DiskRegionSnapshot blockBitmapRegion = makeDiskRegion(
        "block_bitmap",
        "Block 位图",
        "metadata",
        "metadata-used",
        "记录数据块是否被分配；这 20 个块是位图自身占用的元数据块。",
        BLOCKBIITMAP_START_ADDR,
        blockBitmapBlocks
    );
    DiskRegionSnapshot inodeTableRegion = makeDiskRegion(
        "inode_table",
        "Inode 表",
        "metadata",
        "metadata-partial",
        "保存 inode 结构体；每个磁盘块可容纳多个 inode 记录。",
        INODE_START_ADDR,
        inodeTableBlocks
    );
    DiskRegionSnapshot dataRegion = makeDiskRegion(
        "data_blocks",
        "数据块区",
        "data",
        "from-block-bitmap",
        "用户目录和文件内容所在区域，占用状态来自 block bitmap。",
        BLOCK_STARTADDR,
        sb->s_BLOCK_NUM
    );

    snapshot.diskLayout.blockSize = BLOCK_SIZE;
    snapshot.diskLayout.metadataBlocks = dataStartBlock;
    snapshot.diskLayout.dataBlocks = sb->s_BLOCK_NUM;
    snapshot.diskLayout.totalBlocks = snapshot.diskLayout.metadataBlocks + snapshot.diskLayout.dataBlocks;
    snapshot.diskLayout.regions.push_back(superblockRegion);
    snapshot.diskLayout.regions.push_back(inodeBitmapRegion);
    snapshot.diskLayout.regions.push_back(blockBitmapRegion);
    snapshot.diskLayout.regions.push_back(inodeTableRegion);
    snapshot.diskLayout.regions.push_back(dataRegion);
    snapshot.diskLayout.blocks.reserve(snapshot.diskLayout.totalBlocks);

    for (unsigned int i = 0; i < superblockRegion.blockCount; ++i) {
        snapshot.diskLayout.blocks.push_back(makeDiskBlock(superblockRegion.startBlock + i, superblockRegion, "reserved"));
    }
    for (unsigned int i = 0; i < inodeBitmapRegion.blockCount; ++i) {
        snapshot.diskLayout.blocks.push_back(makeDiskBlock(inodeBitmapRegion.startBlock + i, inodeBitmapRegion, "metadata-used"));
    }
    for (unsigned int i = 0; i < blockBitmapRegion.blockCount; ++i) {
        snapshot.diskLayout.blocks.push_back(makeDiskBlock(blockBitmapRegion.startBlock + i, blockBitmapRegion, "metadata-used"));
    }

    const unsigned int recordsPerInodeBlock = BLOCK_SIZE / INODE_SIZE;
    for (unsigned int blockOffset = 0; blockOffset < inodeTableRegion.blockCount; ++blockOffset) {
        DiskBlockSnapshot block = makeDiskBlock(inodeTableRegion.startBlock + blockOffset, inodeTableRegion, "metadata-empty");
        const unsigned int firstInode = blockOffset * recordsPerInodeBlock;
        const unsigned int remainingInodes = firstInode < sb->s_INODE_NUM ? sb->s_INODE_NUM - firstInode : 0;
        block.totalRecords = remainingInodes < recordsPerInodeBlock ? remainingInodes : recordsPerInodeBlock;
        for (unsigned int i = 0; i < block.totalRecords; ++i) {
            if (snapshot.inodeBitmap[firstInode + i] != 0) {
                ++block.usedRecords;
            }
        }
        if (block.usedRecords == block.totalRecords && block.totalRecords > 0) {
            block.state = "metadata-used";
        } else if (block.usedRecords > 0) {
            block.state = "metadata-partial";
        }
        snapshot.diskLayout.blocks.push_back(block);
    }

    for (unsigned int dataIndex = 0; dataIndex < sb->s_BLOCK_NUM; ++dataIndex) {
        DiskBlockSnapshot block = makeDiskBlock(dataRegion.startBlock + dataIndex, dataRegion, snapshot.blockBitmap[dataIndex] ? "data-used" : "data-free");
        block.dataBlockIndex = static_cast<int>(dataIndex);
        block.dataBlockAddress = static_cast<int>(BLOCK_STARTADDR + dataIndex * BLOCK_SIZE);
        snapshot.diskLayout.blocks.push_back(block);
    }

    snapshot.inodeTable.reserve(snapshot.layout.totalInodes);
    for (unsigned int i = 0; i < snapshot.layout.totalInodes; ++i) {
        InodeSnapshot inodeSnapshot;
        iNode inode;
        const unsigned int inodeAddress = INODE_START_ADDR + i * INODE_SIZE;

        inodeSnapshot.index = i;
        inodeSnapshot.address = inodeAddress;
        inodeSnapshot.used = m_fs.storage.inode_bitmap[i];
        inodeSnapshot.indirectBlock = -1;

        fseek(fr, inodeAddress, SEEK_SET);
        fread(&inode, sizeof(iNode), 1, fr);

        inodeSnapshot.inodeDetail.inode_id = inode.inode_id;
        inodeSnapshot.inodeDetail.inode_mode = inode.inode_mode;
        inodeSnapshot.inodeDetail.inode_cnt = inode.inode_cnt;
        inodeSnapshot.inodeDetail.user_name = boundedCString(inode.user_name, sizeof(inode.user_name));
        inodeSnapshot.inodeDetail.user_group = boundedCString(inode.user_group, sizeof(inode.user_group));
        inodeSnapshot.inodeDetail.inode_size = inode.inode_size;
        inodeSnapshot.inodeDetail.inode_dirblock.reserve(10);
        for (int blockIndex = 0; blockIndex < 10; ++blockIndex) {
            inodeSnapshot.inodeDetail.inode_dirblock.push_back(inode.inode_dirblock[blockIndex]);
        }
        inodeSnapshot.inodeDetail.inode_indirect_block_first = inode.inode_indirect_block_first;

        if (inodeSnapshot.used) {
            inodeSnapshot.type = ((inode.inode_mode >> 9) & 1) == 1 ? "dir" : "file";
            inodeSnapshot.mode = inode.inode_mode;
            inodeSnapshot.permissions = permissionsFromMode(inode.inode_mode);
            inodeSnapshot.owner = inode.user_name;
            inodeSnapshot.group = inode.user_group;
            inodeSnapshot.size = inode.inode_size;
            inodeSnapshot.linkCount = inode.inode_cnt;

            for (int blockIndex = 0; blockIndex < 10; ++blockIndex) {
                const int blockAddress = inode.inode_dirblock[blockIndex];
                if (isValidDataBlockAddress(blockAddress)) {
                    inodeSnapshot.directBlocks.push_back(toDataBlockIndex(blockAddress));
                }
            }

            if (isValidDataBlockAddress(inode.inode_indirect_block_first)) {
                inodeSnapshot.indirectBlock = static_cast<int>(toDataBlockIndex(inode.inode_indirect_block_first));
            }
        }

        snapshot.inodeTable.push_back(inodeSnapshot);
    }

    return snapshot;
}

std::vector<int> FileSystemAdapter::GetGroupedLinkingStack(const std::string& source, int groupBlockAddress) const {
    std::vector<int> stack;
    SuperBlock* sb = m_fs.storage.superBlock;
    stack.reserve(sb->s_blocks_per_group);

    if (source == "SuperBlock") {
        for (unsigned int i = 0; i < sb->s_blocks_per_group; ++i) {
            stack.push_back(sb->s_free[i]);
        }
        return stack;
    }

    if (!isValidDataBlockAddress(groupBlockAddress)) {
        return stack;
    }

    int rawStack[BLOCKS_PER_GROUP] = {0};
    FILE* fr = m_fs.storage.image.get_file_read();
    fseek(fr, groupBlockAddress, SEEK_SET);
    const std::size_t readCount = fread(rawStack, sizeof(rawStack), 1, fr);
    if (readCount != 1) {
        return stack;
    }

    for (unsigned int i = 0; i < sb->s_blocks_per_group; ++i) {
        stack.push_back(rawStack[i]);
    }
    return stack;
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
