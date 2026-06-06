#ifndef FLYFLYUFS_WS_SERVER_H
#define FLYFLYUFS_WS_SERVER_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

class FileSystemAdapter;

struct WsSession {
    bool authenticated = false;
    std::string username;
};

class WebSocketServer {
public:
    explicit WebSocketServer(FileSystemAdapter& adapter);
    ~WebSocketServer();

    bool start(int port);
    void stop();

private:
    typedef websocketpp::server<websocketpp::config::asio> WsServer;
    typedef websocketpp::connection_hdl connection_hdl;

    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, WsServer::message_ptr msg);

    std::string handleCommand(const std::string& command, const std::string& params);

    FileSystemAdapter& m_adapter;
    WsServer m_server;
    std::mutex m_mutex;
    std::atomic<bool> m_shutdownRequested;

    typedef std::map<connection_hdl, WsSession,
                     std::owner_less<connection_hdl>> SessionMap;
    SessionMap m_sessions;
};

#endif // FLYFLYUFS_WS_SERVER_H
