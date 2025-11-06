#include "ServerConfig.hpp"
#include <sstream>

ServerConfig ServerConfig::load_from(const std::shared_ptr<ConfigManager>& config) {
    ServerConfig server_config;
    
    server_config.port = config->get_int("port", 8888);
    server_config.thread_pool_size = config->get_int("thread_pool_size", 4);
    server_config.connection_timeout = config->get_int("connection_timeout", 300);
    server_config.heartbeat_interval = config->get_int("heartbeat_interval", 60);
    server_config.max_message_history = config->get_int("max_message_history", 1000);
    server_config.log_file = config->get_string("log_file", "chat_server.log");
    server_config.console_log = config->get_bool("console_log", true);
    server_config.database_path = config->get_string("database_path", "chat_server.db");
    server_config.web_port = config->get_int("web_port", 8080);
    server_config.enable_web_interface = config->get_bool("enable_web_interface", true);
    
    return server_config;
}

bool ServerConfig::validate() const {
    if (port <= 0 || port > 65535) return false;
    if (thread_pool_size <= 0) return false;
    if (connection_timeout <= 0) return false;
    if (heartbeat_interval <= 0) return false;
    if (web_port <= 0 || web_port > 65535) return false;
    if (port == web_port) return false; // 避免端口冲突
    
    return true;
}

std::unordered_map<std::string, std::string> ServerConfig::to_config_map(const ServerConfig& config) {
    return {
        {"port", std::to_string(config.port)},
        {"thread_pool_size", std::to_string(config.thread_pool_size)},
        {"connection_timeout", std::to_string(config.connection_timeout)},
        {"heartbeat_interval", std::to_string(config.heartbeat_interval)},
        {"max_message_history", std::to_string(config.max_message_history)},
        {"log_file", config.log_file},
        {"console_log", config.console_log ? "true" : "false"},
        {"database_path", config.database_path},
        {"web_port", std::to_string(config.web_port)},
        {"enable_web_interface", config.enable_web_interface ? "true" : "false"}
    };
}

std::string ServerConfig::to_string() const {
    std::stringstream ss;
    ss << "ServerConfig {"
       << " port=" << port
       << ", thread_pool_size=" << thread_pool_size
       << ", connection_timeout=" << connection_timeout
       << ", heartbeat_interval=" << heartbeat_interval
       << ", max_message_history=" << max_message_history
       << ", log_file=" << log_file
       << ", console_log=" << console_log
       << ", database_path=" << database_path
       << ", web_port=" << web_port
       << ", enable_web_interface=" << enable_web_interface
       << " }";
    return ss.str();
}