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

    explicit FileSystemAdapter(FileSystem& fs);

    // ---- Session ----------------------------------------------------------
    /// Attempt a single login.  Does NOT loop; returns true on success.
    bool Login(const std::string& username, const std::string& password);
    bool IsLoggedIn() const;
    std::string GetCurrentUser() const;
    std::string GetCurrentPath() const;

    // ---- Directory listing ------------------------------------------------
    /// Returns the formatted "ls" output for the current directory.
    std::string ListDirectory();

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

    // ---- Permissions ------------------------------------------------------
    std::string ChangeMode(const std::string& name, int mode);

    // ---- User management --------------------------------------------------
    std::string AddUser(const std::string& username);
    std::string DeleteUser(const std::string& username);

    // ---- Disk usage -------------------------------------------------------
    DiskUsage GetDiskUsage() const;

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
