#include "network/connection_manager.hpp"
#include <iostream>
#include <chrono>

ConnectionManager::ConnectionManager() {}

ConnectionManager::~ConnectionManager() {
    std::lock_guard lock(connections_mutex_);
    connections_.clear();
}

void ConnectionManager::add_connection(int socket_fd) {
    std::lock_guard lock(connections_mutex_);
    
    ConnectionInfo info;
    info.last_activity = std::chrono::steady_clock::now();
    info.create_time = std::chrono::steady_clock::now();
    
    connections_[socket_fd] = info;
    total_connections_++;
    
    std::cout << "连接已添加: FD " << socket_fd 
              << " (总连接数: " << total_connections_ << ")" << std::endl;
}

void ConnectionManager::update_activity(int socket_fd) {
    std::lock_guard lock(connections_mutex_);
    
    auto it = connections_.find(socket_fd);
    if (it != connections_.end()) {
        it->second.last_activity = std::chrono::steady_clock::now();
    }
}

void ConnectionManager::remove_connection(int socket_fd) {
    std::lock_guard lock(connections_mutex_);
    
    auto it = connections_.find(socket_fd);
    if (it != connections_.end()) {
        connections_.erase(it);
        total_connections_--;
        
        std::cout << "连接已移除: FD " << socket_fd 
                  << " (剩余连接数: " << total_connections_ << ")" << std::endl;
    }
}

bool ConnectionManager::check_connection_timeout(int socket_fd, int timeout_seconds) {
    std::lock_guard lock(connections_mutex_);
    
    auto it = connections_.find(socket_fd);
    if (it == connections_.end()) {
        return true; // 连接不存在，视为超时
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - it->second.last_activity);
    
    return duration.count() > timeout_seconds;
}

void ConnectionManager::check_all_connections(int timeout_seconds) {
    std::lock_guard lock(connections_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = connections_.begin(); it != connections_.end(); ) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_activity);
        
        if (duration.count() > timeout_seconds) {
            std::cout << "连接超时: FD " << it->first 
                      << " (空闲: " << duration.count() << "秒)" << std::endl;
            it = connections_.erase(it);
            total_connections_--;
        } else {
            ++it;
        }
    }
}

std::vector<int> ConnectionManager::get_timed_out_connections(int timeout_seconds) {
    std::lock_guard lock(connections_mutex_);
    std::vector<int> timed_out;
    
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& conn : connections_) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - conn.second.last_activity);
        
        if (duration.count() > timeout_seconds) {
            timed_out.push_back(conn.first);
        }
    }
    
    return timed_out;
}

size_t ConnectionManager::get_connection_count() const {
    return total_connections_;
}