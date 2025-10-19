#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include "network/network_manager.hpp"
#include "managers/user_manager.hpp"
#include "thread/thread_pool.hpp"
#include "managers/message_history.hpp"
#include "network/connection_manager.hpp"
#include "utils/config_manager.hpp"
#include "utils/logger.hpp"
#include "database/database_manager.hpp"
#include "web/web_server.hpp"
#include "web/web_api.hpp"
#include "types/chat_types.hpp"

struct ServerConfig {
    int port = 8888;
    int thread_pool_size = 4;
    int connection_timeout = 300;
    int heartbeat_interval = 60;
    size_t max_message_history = 1000;
    std::string log_file = "chat_server.log";
    bool console_log = true;
    std::string database_path = "chat_server.db";
    int web_port = 8080;
    bool enable_web_interface = true;
};

class ChatServer {
public:
    ChatServer();
    ~ChatServer();
    
    bool initialize(const std::string& config_file = "server.conf");
    void run();
    void shutdown();
    bool is_running() const { return running_; }
    
private:
    // 初始化方法
    bool load_config(const std::string& config_file);
    bool initialize_database();
    bool initialize_network();
    bool initialize_web_interface();
    bool initialize_managers();
    
    // 线程函数
    void heartbeat_thread();
    void input_thread_func();
    void main_loop();
    
    // 客户端处理
    void handle_client(int client_fd);
    void handle_client_message(int client_fd, const std::string& message);
    
    // 工具函数
    std::vector<std::string> split_string(const std::string& str, char delimiter);
    
    // 信号处理
    static void signal_handler(int signal);
    
    // 成员变量
    std::atomic<bool> running_;
    ServerConfig config_;
    
    // 核心组件
    std::unique_ptr<NetworkManager> network_;
    std::unique_ptr<UserManager> user_manager_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<MessageHistory> message_history_;
    std::unique_ptr<ConnectionManager> connection_manager_;
    std::unique_ptr<WebServer> web_server_;
    
    // 线程
    std::thread heartbeat_thread_;
    std::thread input_thread_;
    std::thread main_thread_;
    
    // 单例组件（通过引用访问）
    DatabaseManager& db_;
    Logger& logger_;
    ConfigManager& config_manager_;
    
    // 防止拷贝
    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;
};