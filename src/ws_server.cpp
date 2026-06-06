#include "ws_server.h"
#include "filesystem_adapter.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <functional>
#include <chrono>
#include <thread>

// Undef Windows macros that conflict with FileSystemAdapter method names
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif
#ifdef CreateFile
#undef CreateFile
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif

using json = nlohmann::json;

WebSocketServer::WebSocketServer(FileSystemAdapter& adapter)
    : m_adapter(adapter), m_shutdownRequested(false)
{
    m_server.init_asio();

    m_server.set_open_handler(
        std::bind(&WebSocketServer::on_open, this, std::placeholders::_1));
    m_server.set_close_handler(
        std::bind(&WebSocketServer::on_close, this, std::placeholders::_1));
    m_server.set_message_handler(
        std::bind(&WebSocketServer::on_message, this,
                  std::placeholders::_1, std::placeholders::_2));
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(int port)
{
    try {
        m_server.set_reuse_addr(true);
        m_server.listen(port);
        m_server.start_accept();
        std::cout << "WebSocket server listening on port " << port << std::endl;
        m_server.run();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "WebSocket server error: " << e.what() << std::endl;
        return false;
    }
}

void WebSocketServer::stop()
{
    try {
        m_server.stop_listening();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto conns = m_sessions;
            m_sessions.clear();
            for (auto& pair : conns) {
                try {
                    m_server.close(pair.first,
                                   websocketpp::close::status::going_away,
                                   "Server shutting down");
                } catch (...) {}
            }
        }
        m_server.stop();
    } catch (...) {}
}

void WebSocketServer::on_open(websocketpp::connection_hdl hdl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[hdl] = WsSession();
    std::cout << "WebSocket client connected" << std::endl;
}

void WebSocketServer::on_close(websocketpp::connection_hdl hdl)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.erase(hdl);
    std::cout << "WebSocket client disconnected" << std::endl;
}

void WebSocketServer::on_message(websocketpp::connection_hdl hdl,
                                  WsServer::message_ptr msg)
{
    std::string payload = msg->get_payload();
    std::cout << "Received: " << payload << std::endl;

    json response;
    try {
        json request = json::parse(payload);

        // id can be string or number from frontend
        std::string id;
        if (request.contains("id")) {
            if (request["id"].is_number()) {
                id = std::to_string(request["id"].get<int>());
            } else {
                id = request["id"].get<std::string>();
            }
        }
        std::string command = request.value("command", "");
        std::string params = request.contains("params") ? request["params"].dump() : "{}";

        response["type"] = "response";
        response["id"] = id;

        try {
            std::string result = handleCommand(command, params);
            response["status"] = "ok";
            response["data"] = json::parse(result);
        } catch (const std::exception& e) {
            response["status"] = "error";
            response["message"] = e.what();
        }

    } catch (const std::exception& e) {
        response["type"] = "error";
        response["message"] = std::string("Error: ") + e.what();
    }

    try {
        m_server.send(hdl, response.dump(), msg->get_opcode());
    } catch (const std::exception& e) {
        std::cerr << "Send error: " << e.what() << std::endl;
    }

    if (m_shutdownRequested.exchange(false)) {
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            stop();
        }).detach();
    }
}

std::string WebSocketServer::handleCommand(const std::string& command, const std::string& paramsStr)
{
    json params = json::parse(paramsStr);
    json result;

    if (command == "login") {
        std::string username = params.value("username", "");
        std::string password = params.value("password", "");
        bool success = m_adapter.Login(username, password);
        result["success"] = success;
        if (success) {
            result["user"] = m_adapter.GetCurrentUser();
            result["path"] = m_adapter.GetCurrentPath();
        } else {
            result["error"] = "Invalid credentials";
        }
    }
    else if (command == "ls") {
        std::string listing = m_adapter.ListDirectory();
        result["output"] = listing;
        result["path"] = m_adapter.GetCurrentPath();
    }
    else if (command == "properties") {
        std::string name = params.value("name", "");
        auto properties = m_adapter.GetEntryProperties(name);
        result["success"] = properties.found;
        if (!properties.found) {
            result["error"] = properties.error;
        } else {
            result["properties"] = {
                {"name", properties.name},
                {"type", properties.type},
                {"location", properties.location},
                {"size", properties.size},
                {"permissions", properties.permissions},
                {"permissionOctal", properties.permissionOctal},
                {"owner", properties.owner},
                {"group", properties.group},
                {"inodeAddress", properties.inodeAddress},
                {"inodeId", properties.inodeId},
                {"linkCount", properties.linkCount},
                {"modified", properties.modified.empty() ? json(nullptr) : json(properties.modified)}
            };
        }
    }
    else if (command == "cd") {
        std::string path = params.value("path", "/");
        std::string newPath = m_adapter.ChangeDirectory(path);
        // Check if the result looks like an error (contains non-path characters)
        // Error messages from FindDir are in Chinese, paths start with /
        if (!newPath.empty() && newPath[0] == '/') {
            result["path"] = newPath;
        } else {
            // Likely an error message
            result["error"] = newPath;
            result["path"] = m_adapter.GetCurrentPath();
        }
    }
    else if (command == "mkdir") {
        std::string name = params.value("name", "");
        std::string res = m_adapter.MakeDirectory(name);
        result["output"] = res;
    }
    else if (command == "rmdir") {
        std::string name = params.value("name", "");
        std::string res = m_adapter.RemoveDirectory(name);
        result["output"] = res;
    }
    else if (command == "touch") {
        std::string name = params.value("name", "");
        std::string res = m_adapter.CreateFile(name);
        if (res.find("File created:") == 0) {
            result["success"] = true;
            result["output"] = res;
        } else {
            result["success"] = false;
            result["error"] = res;
        }
    }
    else if (command == "rm") {
        std::string name = params.value("name", "");
        std::string res = m_adapter.DeleteFile(name);
        result["output"] = res;
    }
    else if (command == "cat") {
        std::string name = params.value("name", "");
        std::string content = m_adapter.ReadFile(name);
        result["content"] = content;
    }
    else if (command == "write") {
        std::string name = params.value("name", "");
        std::string content = params.value("content", "");
        std::string res = m_adapter.WriteFile(name, content);
        if (res.empty()) {
            result["output"] = "File saved: " + name;
        } else {
            result["error"] = res;
        }
    }
    else if (command == "copy") {
        std::string sourcePath = params.value("sourcePath", "");
        std::string targetPath = params.value("targetPath", "");
        std::string res = m_adapter.CopyEntry(sourcePath, targetPath);
        if (res.find("Copied:") == 0) {
            result["success"] = true;
            result["output"] = res;
        } else {
            result["success"] = false;
            result["error"] = res;
        }
    }
    else if (command == "move") {
        std::string sourcePath = params.value("sourcePath", "");
        std::string targetPath = params.value("targetPath", "");
        std::string res = m_adapter.MoveEntry(sourcePath, targetPath);
        if (res.find("Moved:") == 0) {
            result["success"] = true;
            result["output"] = res;
        } else {
            result["success"] = false;
            result["error"] = res;
        }
    }
    else if (command == "upload") {
        std::string targetPath = params.value("targetPath", "");
        std::string name = params.value("name", "");
        std::string content = params.value("content", "");
        std::string res = m_adapter.UploadFile(targetPath, name, content);
        if (res.find("Uploaded:") == 0) {
            result["success"] = true;
            result["output"] = res;
        } else {
            result["success"] = false;
            result["error"] = res;
        }
    }
    else if (command == "chmod") {
        std::string name = params.value("name", "");
        int mode = params.value("mode", 0644);
        std::string res = m_adapter.ChangeMode(name, mode);
        result["output"] = res;
    }
    else if (command == "help") {
        result["output"] = m_adapter.Help();
    }
    else if (command == "clear") {
        result["output"] = "";
        result["action"] = "clear";
    }
    else if (command == "exit") {
        std::string res = m_adapter.Logout();
        result["output"] = res.empty() ? "Logged out" : res;
        result["loggedOut"] = true;
    }
    else if (command == "format") {
        std::string confirm = params.value("confirm", "");
        if (m_adapter.GetCurrentUser() != "root") {
            result["error"] = "Permission Dennied";
        } else if (confirm != "FORMAT") {
            result["error"] = "format requires confirm: FORMAT";
        } else {
            result["output"] = m_adapter.FormatAndLogout();
            result["formatted"] = true;
            result["loggedOut"] = true;
        }
    }
    else if (command == "pwd") {
        result["path"] = m_adapter.GetCurrentPath();
    }
    else if (command == "useradd") {
        std::string username = params.value("username", "");
        std::string password = params.value("password", "");
        std::string res = m_adapter.AddUser(username, password);
        result["output"] = res;
    }
    else if (command == "userdel") {
        std::string username = params.value("username", "");
        std::string res = m_adapter.DeleteUser(username);
        result["output"] = res;
    }
    else if (command == "disk_usage") {
        auto usage = m_adapter.GetDiskUsage();
        result["total_blocks"] = usage.total_blocks;
        result["used_blocks"] = usage.used_blocks;
        result["free_blocks"] = usage.free_blocks;
        result["total_inodes"] = usage.total_inodes;
        result["free_inodes"] = usage.free_inodes;
    }
    else if (command == "storage_inspection") {
        auto snapshot = m_adapter.GetStorageInspectionSnapshot();

        result["layout"] = {
            {"blockSize", snapshot.layout.blockSize},
            {"inodeSize", snapshot.layout.inodeSize},
            {"totalBlocks", snapshot.layout.totalBlocks},
            {"totalInodes", snapshot.layout.totalInodes},
            {"superblockStart", snapshot.layout.superblockStart},
            {"inodeBitmapStart", snapshot.layout.inodeBitmapStart},
            {"blockBitmapStart", snapshot.layout.blockBitmapStart},
            {"inodeTableStart", snapshot.layout.inodeTableStart},
            {"dataBlockStart", snapshot.layout.dataBlockStart}
        };

        result["summary"] = {
            {"usedBlocks", snapshot.summary.usedBlocks},
            {"freeBlocks", snapshot.summary.freeBlocks},
            {"usedInodes", snapshot.summary.usedInodes},
            {"freeInodes", snapshot.summary.freeInodes}
        };

        result["bitmaps"]["blocks"] = snapshot.blockBitmap;
        result["bitmaps"]["inodes"] = snapshot.inodeBitmap;

        result["diskLayout"] = {
            {"blockSize", snapshot.diskLayout.blockSize},
            {"totalBlocks", snapshot.diskLayout.totalBlocks},
            {"metadataBlocks", snapshot.diskLayout.metadataBlocks},
            {"dataBlocks", snapshot.diskLayout.dataBlocks}
        };
        result["diskLayout"]["regions"] = json::array();
        for (const auto& region : snapshot.diskLayout.regions) {
            result["diskLayout"]["regions"].push_back({
                {"id", region.id},
                {"label", region.label},
                {"kind", region.kind},
                {"state", region.state},
                {"description", region.description},
                {"startBlock", region.startBlock},
                {"blockCount", region.blockCount},
                {"startByte", region.startByte},
                {"endByte", region.endByte}
            });
        }
        result["diskLayout"]["blocks"] = json::array();
        for (const auto& block : snapshot.diskLayout.blocks) {
            result["diskLayout"]["blocks"].push_back({
                {"index", block.index},
                {"byteOffset", block.byteOffset},
                {"region", block.region},
                {"regionLabel", block.regionLabel},
                {"kind", block.kind},
                {"state", block.state},
                {"dataBlockIndex", block.dataBlockIndex},
                {"dataBlockAddress", block.dataBlockAddress},
                {"usedRecords", block.usedRecords},
                {"totalRecords", block.totalRecords}
            });
        }

        result["groupedLinking"] = {
            {"blocksPerGroup", snapshot.groupedLinking.blocksPerGroup},
            {"freeBlockCount", snapshot.groupedLinking.freeBlockCount},
            {"currentTop", snapshot.groupedLinking.currentTop},
            {"currentGroupBlockAddress", snapshot.groupedLinking.currentGroupBlockAddress},
            {"currentGroupBlockIndex", snapshot.groupedLinking.currentGroupBlockIndex},
            {"truncated", snapshot.groupedLinking.truncated}
        };
        result["groupedLinking"]["groups"] = json::array();
        for (const auto& group : snapshot.groupedLinking.groups) {
            const bool hasFreeBlocks = !group.freeBlockIndexes.empty();
            result["groupedLinking"]["groups"].push_back({
                {"order", group.order},
                {"source", group.source},
                {"groupBlockAddress", group.groupBlockAddress},
                {"groupBlockIndex", group.groupBlockIndex},
                {"nextGroupAddress", group.nextGroupAddress},
                {"nextGroupIndex", group.nextGroupIndex},
                {"top", group.top},
                {"stackLength", group.stack.size()},
                {"freeBlockCount", group.freeBlockIndexes.size()},
                {"firstFreeBlockIndex", hasFreeBlocks ? static_cast<int>(group.freeBlockIndexes.front()) : -1},
                {"lastFreeBlockIndex", hasFreeBlocks ? static_cast<int>(group.freeBlockIndexes.back()) : -1}
            });
        }

        result["inodeTable"] = json::array();
        for (const auto& inode : snapshot.inodeTable) {
            json row;
            row["index"] = inode.index;
            row["address"] = inode.address;
            row["used"] = inode.used;
            row["inodeDetail"] = {
                {"inode_id", inode.inodeDetail.inode_id},
                {"inode_mode", inode.inodeDetail.inode_mode},
                {"inode_cnt", inode.inodeDetail.inode_cnt},
                {"user_name", inode.inodeDetail.user_name},
                {"user_group", inode.inodeDetail.user_group},
                {"inode_size", inode.inodeDetail.inode_size},
                {"inode_dirblock", inode.inodeDetail.inode_dirblock},
                {"inode_indirect_block_first", inode.inodeDetail.inode_indirect_block_first}
            };

            if (inode.used) {
                row["type"] = inode.type;
                row["mode"] = inode.mode;
                row["permissions"] = inode.permissions;
                row["owner"] = inode.owner;
                row["group"] = inode.group;
                row["size"] = inode.size;
                row["linkCount"] = inode.linkCount;
                row["directBlocks"] = inode.directBlocks;
                row["indirectBlock"] = inode.indirectBlock;
            }

            result["inodeTable"].push_back(row);
        }
    }
    else if (command == "grouped_linking_stack") {
        std::string source = params.value("source", "");
        int groupBlockAddress = params.value("groupBlockAddress", -1);
        auto stack = m_adapter.GetGroupedLinkingStack(source, groupBlockAddress);
        result["source"] = source;
        result["groupBlockAddress"] = groupBlockAddress;
        result["stack"] = stack;
    }
    else if (command == "shutdown") {
        result["success"] = true;
        result["message"] = "Server is shutting down";
        m_shutdownRequested.store(true);
    }
    else if (command == "directory_sizes") {
        result["output"] = "directory_sizes not implemented";
    }
    else {
        result["echo"] = true;
        result["command"] = command;
        result["params"] = params;
    }

    return result.dump();
}
