/**
 * C++ Adapter Reference — FlyflyUFS WebSocket Backend
 * 
 * This file shows how to map frontend JSON requests to your existing C++ FileSystem methods.
 * It extends the existing ws_server.cpp with full command routing.
 * 
 * IMPORTANT: This is a REFERENCE ONLY. Do not compile this file directly.
 * Instead, integrate these patterns into your existing ws_server.cpp.
 */

#include "ws_server.h"
#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <functional>

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════════
// Command Router — Maps command strings to handler functions
// ═══════════════════════════════════════════════════════════════════════════════

class CommandRouter {
public:
    using Handler = std::function<json(const json& params, WsSession& session)>;
    
    void registerCommand(const std::string& command, Handler handler) {
        m_handlers[command] = handler;
    }
    
    json dispatch(const std::string& command, const json& params, WsSession& session) {
        auto it = m_handlers.find(command);
        if (it == m_handlers.end()) {
            return makeError(0, 1001, "Unknown command: " + command);
        }
        try {
            return it->second(params, session);
        } catch (const std::exception& e) {
            return makeError(0, 1006, std::string("Filesystem error: ") + e.what());
        }
    }

private:
    std::map<std::string, Handler> m_handlers;
    
    json makeError(int id, int code, const std::string& message) {
        return {
            {"type", "error"},
            {"id", id},
            {"code", code},
            {"message", message}
        };
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Response Helpers
// ═══════════════════════════════════════════════════════════════════════════════

json makeSuccessResponse(int id, const json& data = nullptr) {
    return {
        {"type", "response"},
        {"id", id},
        {"status", 200},
        {"data", data}
    };
}

json makeErrorResponse(int id, int code, const std::string& message) {
    return {
        {"type", "error"},
        {"id", id},
        {"code", code},
        {"message", message}
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// Command Implementations
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Register all commands with the router.
 * Call this once during server initialization.
 */
void registerCommands(CommandRouter& router, FileSystem& fs) {
    
    // ── login ────────────────────────────────────────────────────────────────
    router.registerCommand("login", [&fs](const json& params, WsSession& session) -> json {
        // Validate params
        if (!params.contains("username") || !params.contains("password")) {
            return makeErrorResponse(0, 1002, "Missing username or password");
        }
        
        std::string username = params["username"];
        std::string password = params["password"];
        
        // Call your FileSystem's authentication method
        // Adjust based on your actual API:
        //   bool success = fs.authenticate(username, password);
        //   or: int userId = fs.login(username, password);
        
        // For now, accept any login (adjust to your auth logic)
        bool success = true; // Replace with: fs.validateCredentials(username, password);
        
        if (!success) {
            return makeErrorResponse(0, 401, "Invalid credentials");
        }
        
        // Set session state
        session.authenticated = true;
        session.username = username;
        
        // Navigate to user's home directory
        // Adjust based on your filesystem structure:
        //   fs.cd("/home/" + username);
        //   or: fs.cd("/root"); for root user
        
        return makeSuccessResponse(0, {
            {"username", username},
            {"group", "root"} // Adjust based on user's actual group
        });
    });
    
    // ── ls ────────────────────────────────────────────────────────────────────
    router.registerCommand("ls", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        // Call your FileSystem's list directory method
        // Adjust based on your actual API:
        //   auto entries = fs.ls();
        //   or: auto entries = fs.listCurrentDirectory();
        //   or: std::vector<FileInfo> entries = fs.readdir(".");
        
        // Example: Convert your C++ structs to JSON
        json result = json::array();
        
        // Pseudo-code - replace with your actual implementation:
        /*
        auto entries = fs.ls();
        for (const auto& entry : entries) {
            result.push_back({
                {"name", entry.name},
                {"type", entry.isDirectory ? "dir" : "file"},
                {"size", entry.size},
                {"permissions", formatPermissions(entry.mode)},
                {"inodeAddr", entry.inodeNumber},
                {"owner", entry.owner},
                {"group", entry.group}
            });
        }
        */
        
        // Placeholder - replace with actual filesystem call
        return makeSuccessResponse(0, result);
    });
    
    // ── cd ────────────────────────────────────────────────────────────────────
    router.registerCommand("cd", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing directory name");
        }
        
        std::string name = params["name"];
        
        // Call your FileSystem's change directory method
        // Adjust based on your actual API:
        //   bool success = fs.cd(name);
        //   or: int result = fs.chdir(name.c_str());
        
        // Pseudo-code:
        /*
        bool success = fs.cd(name);
        if (!success) {
            return makeErrorResponse(0, 1003, "Directory not found: " + name);
        }
        */
        
        // Get new current path
        // std::string newPath = fs.pwd();
        std::string newPath = "/root/" + name; // Placeholder
        
        return makeSuccessResponse(0, {{"path", newPath}});
    });
    
    // ── mkdir ─────────────────────────────────────────────────────────────────
    router.registerCommand("mkdir", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing directory name");
        }
        
        std::string name = params["name"];
        
        // Validate name length (MAX_NAME_SIZE = 28)
        if (name.length() > 28) {
            return makeErrorResponse(0, 1007, "Name too long (max 28 characters)");
        }
        
        // Call your FileSystem's create directory method
        // Adjust based on your actual API:
        //   bool success = fs.mkdir(name);
        //   or: int result = fs.mkdir(name.c_str(), 0755);
        
        // Pseudo-code:
        /*
        bool success = fs.mkdir(name);
        if (!success) {
            return makeErrorResponse(0, 1004, "Directory already exists: " + name);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── rmdir ─────────────────────────────────────────────────────────────────
    router.registerCommand("rmdir", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing directory name");
        }
        
        std::string name = params["name"];
        
        // Call your FileSystem's remove directory method
        // Adjust based on your actual API:
        //   bool success = fs.rmdir(name);
        //   or: int result = fs.rmdir(name.c_str());
        
        // Pseudo-code:
        /*
        bool success = fs.rmdir(name);
        if (!success) {
            // Could be not found or not empty
            return makeErrorResponse(0, 1008, "Cannot remove directory (not empty or not found)");
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── touch ─────────────────────────────────────────────────────────────────
    router.registerCommand("touch", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing file name");
        }
        
        std::string name = params["name"];
        
        // Validate name length (MAX_NAME_SIZE = 28)
        if (name.length() > 28) {
            return makeErrorResponse(0, 1007, "Name too long (max 28 characters)");
        }
        
        // Call your FileSystem's create file method
        // Adjust based on your actual API:
        //   bool success = fs.touch(name);
        //   or: int fd = fs.open(name.c_str(), O_CREAT | O_WRONLY, 0644);
        //        fs.close(fd);
        
        // Pseudo-code:
        /*
        bool success = fs.createFile(name);
        if (!success) {
            return makeErrorResponse(0, 1004, "File already exists: " + name);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── rm ────────────────────────────────────────────────────────────────────
    router.registerCommand("rm", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing file name");
        }
        
        std::string name = params["name"];
        
        // Call your FileSystem's delete file method
        // Adjust based on your actual API:
        //   bool success = fs.rm(name);
        //   or: int result = fs.unlink(name.c_str());
        
        // Pseudo-code:
        /*
        bool success = fs.removeFile(name);
        if (!success) {
            return makeErrorResponse(0, 1003, "File not found: " + name);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── cat ───────────────────────────────────────────────────────────────────
    router.registerCommand("cat", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name")) {
            return makeErrorResponse(0, 1002, "Missing file name");
        }
        
        std::string name = params["name"];
        
        // Call your FileSystem's read file method
        // Adjust based on your actual API:
        //   std::string content = fs.cat(name);
        //   or: std::string content = fs.readFile(name.c_str());
        
        // Pseudo-code:
        /*
        std::string content;
        bool success = fs.readFile(name, content);
        if (!success) {
            return makeErrorResponse(0, 1003, "File not found: " + name);
        }
        return makeSuccessResponse(0, {{"content", content}});
        */
        
        // Placeholder
        return makeSuccessResponse(0, {{"content", ""}});
    });
    
    // ── chmod ─────────────────────────────────────────────────────────────────
    router.registerCommand("chmod", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("name") || !params.contains("mode")) {
            return makeErrorResponse(0, 1002, "Missing name or mode");
        }
        
        std::string name = params["name"];
        std::string mode = params["mode"];
        
        // Parse octal mode string (e.g., "0755" -> 0755)
        // int modeInt = std::stoi(mode, nullptr, 8);
        
        // Call your FileSystem's chmod method
        // Adjust based on your actual API:
        //   bool success = fs.chmod(name, modeInt);
        //   or: int result = fs.chmod(name.c_str(), modeInt);
        
        // Pseudo-code:
        /*
        bool success = fs.chmod(name, modeInt);
        if (!success) {
            return makeErrorResponse(0, 1003, "File not found: " + name);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── pwd ───────────────────────────────────────────────────────────────────
    router.registerCommand("pwd", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        // Call your FileSystem's get current path method
        // std::string path = fs.pwd();
        std::string path = "/root"; // Placeholder
        
        return makeSuccessResponse(0, {{"path", path}});
    });
    
    // ── useradd ───────────────────────────────────────────────────────────────
    router.registerCommand("useradd", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("username")) {
            return makeErrorResponse(0, 1002, "Missing username");
        }
        
        std::string username = params["username"];
        
        // Call your FileSystem's add user method
        // Adjust based on your actual API:
        //   bool success = fs.addUser(username);
        
        // Pseudo-code:
        /*
        bool success = fs.createUser(username);
        if (!success) {
            return makeErrorResponse(0, 1004, "User already exists: " + username);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── userdel ───────────────────────────────────────────────────────────────
    router.registerCommand("userdel", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        if (!params.contains("username")) {
            return makeErrorResponse(0, 1002, "Missing username");
        }
        
        std::string username = params["username"];
        
        // Call your FileSystem's delete user method
        // Adjust based on your actual API:
        //   bool success = fs.deleteUser(username);
        
        // Pseudo-code:
        /*
        bool success = fs.removeUser(username);
        if (!success) {
            return makeErrorResponse(0, 1003, "User not found: " + username);
        }
        */
        
        return makeSuccessResponse(0, nullptr);
    });
    
    // ── disk_usage ────────────────────────────────────────────────────────────
    router.registerCommand("disk_usage", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        // Call your FileSystem's disk usage method
        // Adjust based on your actual API:
        //   DiskUsage stats = fs.getDiskUsage();
        
        // Pseudo-code:
        /*
        DiskUsage stats = fs.getDiskUsage();
        return makeSuccessResponse(0, {
            {"totalBlocks", stats.totalBlocks},
            {"usedBlocks", stats.usedBlocks},
            {"freeBlocks", stats.freeBlocks},
            {"totalInodes", stats.totalInodes},
            {"usedInodes", stats.usedInodes},
            {"freeInodes", stats.freeInodes},
            {"blockSize", stats.blockSize}
        });
        */
        
        // Placeholder - replace with actual disk usage
        return makeSuccessResponse(0, {
            {"totalBlocks", 10240},
            {"usedBlocks", 256},
            {"freeBlocks", 9984},
            {"totalInodes", 640},
            {"usedInodes", 32},
            {"freeInodes", 608},
            {"blockSize", 512}
        });
    });
    
    // ── directory_sizes ───────────────────────────────────────────────────────
    router.registerCommand("directory_sizes", [&fs](const json& params, WsSession& session) -> json {
        if (!session.authenticated) {
            return makeErrorResponse(0, 401, "Not logged in");
        }
        
        // Call your FileSystem's directory sizes method
        // Adjust based on your actual API:
        //   auto sizes = fs.getDirectorySizes();
        
        // Pseudo-code:
        /*
        auto sizes = fs.getDirectorySizes();
        json result = json::array();
        for (const auto& dir : sizes) {
            result.push_back({
                {"name", dir.name},
                {"size", dir.size},
                {"childCount", dir.childCount}
            });
        }
        return makeSuccessResponse(0, result);
        */
        
        // Placeholder
        return makeSuccessResponse(0, json::array());
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// Updated WebSocket Server with Command Routing
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Add this to your WebSocketServer class:
 * 
 * private:
 *     CommandRouter m_router;
 * 
 * In constructor, call:
 *     registerCommands(m_router, m_filesystem);
 */

/**
 * Updated on_message handler with full command routing.
 * Replace the existing echo handler in ws_server.cpp.
 */
void WebSocketServer::on_message(websocketpp::connection_hdl hdl,
                                  WsServer::message_ptr msg)
{
    std::string payload = msg->get_payload();
    std::cout << "Received: " << payload << std::endl;

    json response;
    try {
        json request = json::parse(payload);
        
        // Validate message envelope
        if (!request.contains("type") || request["type"] != "request") {
            response = makeErrorResponse(0, 400, "Invalid message type (expected 'request')");
        }
        else if (!request.contains("id") || !request["id"].is_number()) {
            response = makeErrorResponse(0, 400, "Missing or invalid request id");
        }
        else if (!request.contains("command") || !request["command"].is_string()) {
            response = makeErrorResponse(request["id"], 400, "Missing or invalid command");
        }
        else {
            int id = request["id"];
            std::string command = request["command"];
            json params = request.value("params", json::object());
            
            // Get session for this connection
            WsSession& session = m_sessions[hdl];
            
            // Check authentication (except for login command)
            if (command != "login" && !session.authenticated) {
                response = makeErrorResponse(id, 401, "Not logged in");
            }
            else {
                // Dispatch to command handler
                response = m_router.dispatch(command, params, session);
                
                // Set the response id to match the request
                if (response.contains("id")) {
                    response["id"] = id;
                }
            }
        }
    } catch (const json::parse_error& e) {
        response = makeErrorResponse(0, 400, std::string("Invalid JSON: ") + e.what());
    } catch (const std::exception& e) {
        response = makeErrorResponse(0, 500, std::string("Server error: ") + e.what());
    }

    try {
        m_server.send(hdl, response.dump(), msg->get_opcode());
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helper: Format Unix Permissions
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Convert Unix mode bits to string like "0755" or "0644".
 */
std::string formatPermissions(mode_t mode) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%04o", mode & 0xFFF);
    return std::string(buf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Integration Checklist
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * To integrate this into your existing codebase:
 * 
 * 1. Add CommandRouter class to a new file (e.g., command_router.h)
 * 
 * 2. Add registerCommands() function to a new file (e.g., commands.cpp)
 * 
 * 3. In WebSocketServer constructor:
 *    - Initialize CommandRouter: m_router(...)
 *    - Call registerCommands(m_router, m_filesystem);
 * 
 * 4. Replace the echo handler in on_message() with the routing logic above
 * 
 * 5. Implement each command handler using your actual FileSystem API:
 *    - Replace pseudo-code comments with real function calls
 *    - Adjust method names to match your FileSystem class
 *    - Handle errors appropriately
 * 
 * 6. Test with the frontend:
 *    - Start your C++ backend: ./build/flyflyufs
 *    - Open frontend/index.html in browser
 *    - The frontend will auto-connect to ws://localhost:9002/ws
 * 
 * Key FileSystem methods you'll need to implement/adjust:
 * 
 *   class FileSystem {
 *   public:
 *       bool authenticate(const std::string& user, const std::string& pass);
 *       std::vector<FileInfo> ls();
 *       bool cd(const std::string& path);
 *       std::string pwd();
 *       bool mkdir(const std::string& name);
 *       bool rmdir(const std::string& name);
 *       bool createFile(const std::string& name);
 *       bool removeFile(const std::string& name);
 *       bool readFile(const std::string& name, std::string& content);
 *       bool chmod(const std::string& name, mode_t mode);
 *       bool addUser(const std::string& username);
 *       bool removeUser(const std::string& username);
 *       DiskUsage getDiskUsage();
 *       std::vector<DirectoryInfo> getDirectorySizes();
 *   };
 */
