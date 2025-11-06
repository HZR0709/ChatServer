#pragma once
#include <string>
#include <memory>
#include "config_manager.hpp"

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
    
    // 从配置管理器加载
    static ServerConfig load_from(const std::shared_ptr<ConfigManager>& config);
    
    static std::shared_ptr<ConfigManager> create_default_config_manager();
    
    static std::unordered_map<std::string, std::string> to_config_map(const ServerConfig& config);

    // 验证配置
    bool validate() const;
    
    // 获取描述
    std::string to_string() const;
};