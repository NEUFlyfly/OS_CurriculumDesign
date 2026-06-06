#ifndef UNIXFILESYSTEM_FILESYSTEM_ADAPTER_H
#define UNIXFILESYSTEM_FILESYSTEM_ADAPTER_H

// Undefine Windows macros that collide with our method names
#ifdef _WIN32
  #ifdef RemoveDirectory
    #undef RemoveDirectory
  #endif
  #ifdef CreateFile
    #undef CreateFile
  #endif
  #ifdef DeleteFile
    #undef DeleteFile
  #endif
#endif

#include <string>
#include <functional>
#include <vector>

class FileSystem;

/// Public API adapter wrapping a FileSystem instance.
/// All output normally written to stdout/cout is captured and returned as strings
/// so it can be transmitted over WebSocket or other channels.
class FileSystemAdapter {
public:
    struct DiskUsage {
        unsigned int total_blocks;
        unsigned int used_blocks;
        unsigned int free_blocks;
        unsigned int total_inodes;
        unsigned int free_inodes;
    };

    struct FileProperties {
        bool found;
        std::string error;
        std::string name;
        std::string type;
        std::string location;
        unsigned int size;
        std::string permissions;
        std::string permissionOctal;
        std::string owner;
        std::string group;
        int inodeAddress;
        unsigned int inodeId;
        unsigned int linkCount;
        std::string modified;
    };

    struct StorageLayout {
        unsigned int blockSize;
        unsigned int inodeSize;
        unsigned int totalBlocks;
        unsigned int totalInodes;
        unsigned int superblockStart;
        unsigned int inodeBitmapStart;
        unsigned int blockBitmapStart;
        unsigned int inodeTableStart;
        unsigned int dataBlockStart;
    };

    struct StorageSummary {
        unsigned int usedBlocks;
        unsigned int freeBlocks;
        unsigned int usedInodes;
        unsigned int freeInodes;
    };

    struct InodeDetail {
        unsigned short inode_id;
        unsigned short inode_mode;
        unsigned short inode_cnt;
        std::string user_name;
        std::string user_group;
        unsigned int inode_size;
        std::vector<int> inode_dirblock;
        int inode_indirect_block_first;
    };

    struct InodeSnapshot {
        unsigned int index;
        unsigned int address;
        bool used;
        std::string type;
        unsigned int mode;
        std::string permissions;
        std::string owner;
        std::string group;
        unsigned int size;
        unsigned int linkCount;
        std::vector<unsigned int> directBlocks;
        int indirectBlock;
        InodeDetail inodeDetail;
    };

    struct GroupedLinkingGroup {
        unsigned int order;
        std::string source;
        int groupBlockAddress;
        int groupBlockIndex;
        int nextGroupAddress;
        int nextGroupIndex;
        int top;
        std::vector<int> stack;
        std::vector<int> freeBlockAddresses;
        std::vector<unsigned int> freeBlockIndexes;
    };

    struct GroupedLinkingSnapshot {
        unsigned int blocksPerGroup;
        unsigned int freeBlockCount;
        int currentTop;
        int currentGroupBlockAddress;
        int currentGroupBlockIndex;
        std::vector<int> superBlockStack;
        std::vector<GroupedLinkingGroup> groups;
        bool truncated;
    };

    struct DiskRegionSnapshot {
        std::string id;
        std::string label;
        std::string kind;
        std::string state;
        std::string description;
        unsigned int startBlock;
        unsigned int blockCount;
        unsigned int startByte;
        unsigned int endByte;
    };

    struct DiskBlockSnapshot {
        unsigned int index;
        unsigned int byteOffset;
        std::string region;
        std::string regionLabel;
        std::string kind;
        std::string state;
        int dataBlockIndex;
        int dataBlockAddress;
        unsigned int usedRecords;
        unsigned int totalRecords;
    };

    struct DiskLayoutSnapshot {
        unsigned int blockSize;
        unsigned int totalBlocks;
        unsigned int metadataBlocks;
        unsigned int dataBlocks;
        std::vector<DiskRegionSnapshot> regions;
        std::vector<DiskBlockSnapshot> blocks;
    };

    struct StorageInspectionSnapshot {
        StorageLayout layout;
        StorageSummary summary;
        std::vector<int> blockBitmap;
        std::vector<int> inodeBitmap;
        std::vector<InodeSnapshot> inodeTable;
        GroupedLinkingSnapshot groupedLinking;
        DiskLayoutSnapshot diskLayout;
    };

    explicit FileSystemAdapter(FileSystem& fs);

    // ---- Session ----------------------------------------------------------
    /// Attempt a single login.  Does NOT loop; returns true on success.
    bool Login(const std::string& username, const std::string& password);
    bool IsLoggedIn() const;
    std::string GetCurrentUser() const;
    std::string GetCurrentPath() const;
    std::string Help();
    std::string Logout();
    std::string FormatAndLogout();

    // ---- Directory listing ------------------------------------------------
    /// Returns the formatted "ls" output for the current directory.
    std::string ListDirectory();
    FileProperties GetEntryProperties(const std::string& name) const;

    // ---- Navigation -------------------------------------------------------
    /// Changes directory.  Returns the new path on success, or an error message.
    std::string ChangeDirectory(const std::string& path);

    // ---- Directory operations ---------------------------------------------
    std::string MakeDirectory(const std::string& name);
    std::string RemoveDirectory(const std::string& name);

    // ---- File operations --------------------------------------------------
    std::string CreateFile(const std::string& name);
    std::string DeleteFile(const std::string& name);
    /// Returns the file content as a string, or an error message.
    std::string ReadFile(const std::string& name);
    /// Writes content to an existing file. Returns empty string on success, error message on failure.
    std::string WriteFile(const std::string& name, const std::string& content);
    std::string CopyEntry(const std::string& sourcePath, const std::string& targetPath);
    std::string MoveEntry(const std::string& sourcePath, const std::string& targetPath);
    std::string UploadFile(const std::string& targetPath, const std::string& name, const std::string& content);

    // ---- Permissions ------------------------------------------------------
    std::string ChangeMode(const std::string& name, int mode);

    // ---- User management --------------------------------------------------
    std::string AddUser(const std::string& username);
    std::string AddUser(const std::string& username, const std::string& password);
    std::string DeleteUser(const std::string& username);

    // ---- Disk usage -------------------------------------------------------
    DiskUsage GetDiskUsage() const;

    // ---- Storage inspection -----------------------------------------------
    StorageInspectionSnapshot GetStorageInspectionSnapshot() const;
    std::vector<int> GetGroupedLinkingStack(const std::string& source, int groupBlockAddress) const;

private:
    FileSystem& m_fs;

    /// Capture everything written to std::cout while fn executes.
    /// Returns the captured text.
    std::string captureCout(std::function<void()> fn);

    /// Capture stdout (printf + cout) via fd-level redirection.
    /// Used for Cat() which mixes printf and cout.
    std::string captureStdout(std::function<void()> fn);
};

#endif // UNIXFILESYSTEM_FILESYSTEM_ADAPTER_H
