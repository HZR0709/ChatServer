#pragma once

#include <unordered_map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    void add_connection(int socket_fd);
    void update_activity(int socket_fd);
    void remove_connection(int socket_fd);
    bool check_connection_timeout(int socket_fd, int timeout_seconds = 300);
    void check_all_connections(int timeout_seconds = 300);
    std::vector<int> get_timed_out_connections(int timeout_seconds = 300);
    size_t get_connection_count() const;
    
private:
    struct ConnectionInfo {
        std::chrono::steady_clock::time_point last_activity;
        std::chrono::steady_clock::time_point create_time;
    };
    
    std::unordered_map<int, ConnectionInfo> connections_;
    mutable std::mutex connections_mutex_;
    std::atomic<size_t> total_connections_{0};
};