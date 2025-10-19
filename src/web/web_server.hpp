#pragma once

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include "network/network_manager.hpp"

class WebServer {
public:
    using HttpHandler = std::function<std::string(const std::map<std::string, std::string>&)>;
    
    WebServer();
    ~WebServer();
    
    bool start(int port);
    void stop();
    bool is_running() const { return running_; }
    
    // 注册HTTP路由
    void get(const std::string& path, HttpHandler handler);
    void post(const std::string& path, HttpHandler handler);
    
private:
    void run();
    void handle_client(int client_fd);
    std::string read_http_request(int client_fd);
    void send_http_response(int client_fd, const std::string& response);
    std::string handle_request(const std::string& request, int client_fd);
    std::map<std::string, std::string> parse_query_params(const std::string& query);
    std::string url_decode(const std::string& str);
    std::string get_mime_type(const std::string& path);
    
    NetworkManager network_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    int port_;
    
    std::map<std::string, HttpHandler> get_handlers_;
    std::map<std::string, HttpHandler> post_handlers_;
    
    // 静态文件服务
    std::string web_root_;
    
    // 防止拷贝
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
};