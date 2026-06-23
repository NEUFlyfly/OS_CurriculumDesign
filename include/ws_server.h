#ifndef FLYFLYUFS_WS_SERVER_H
#define FLYFLYUFS_WS_SERVER_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <atomic>
#include <deque>
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
    void recordCommandTiming(const std::string& command,
                             const std::string& status,
                             double backendMs);
    std::string buildPerformanceStats();

    struct TimingRecord {
        std::string command;
        std::string status;
        double backendMs = 0.0;
    };

    struct TimingAggregate {
        int count = 0;
        double totalMs = 0.0;
        double minMs = 0.0;
        double maxMs = 0.0;
    };

    enum { kTimingRecordLimit = 50 };

    FileSystemAdapter& m_adapter;
    WsServer m_server;
    std::mutex m_mutex;
    std::atomic<bool> m_shutdownRequested;
    std::deque<TimingRecord> m_recentTimingRecords;
    std::map<std::string, TimingAggregate> m_timingAggregates;
    TimingRecord m_slowestTimingRecord;
    bool m_hasSlowestTimingRecord = false;

    typedef std::map<connection_hdl, WsSession,
                     std::owner_less<connection_hdl>> SessionMap;
    SessionMap m_sessions;
};

#endif // FLYFLYUFS_WS_SERVER_H
