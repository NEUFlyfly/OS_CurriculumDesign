#include "ws_server.h"
#include "filesystem_adapter.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <functional>

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
    : m_adapter(adapter)
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
        result["output"] = res;
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
    else if (command == "chmod") {
        std::string name = params.value("name", "");
        int mode = params.value("mode", 0644);
        std::string res = m_adapter.ChangeMode(name, mode);
        result["output"] = res;
    }
    else if (command == "pwd") {
        result["path"] = m_adapter.GetCurrentPath();
    }
    else if (command == "useradd") {
        std::string username = params.value("username", "");
        std::string res = m_adapter.AddUser(username);
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
