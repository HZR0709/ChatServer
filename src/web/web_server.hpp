#pragma once

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include "network/network_manager.hpp"
#include "web/modern_json_parser.hpp"
#include "utils/debug_manager.hpp"

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
    
    // 设置Web根目录
    void set_web_root(const std::string& web_root) { web_root_ = web_root; }
    
    // 获取服务器信息
    int get_port() const { return port_; }
    size_t get_handler_count() const { 
        return get_handlers_.size() + post_handlers_.size(); 
    }

private:
    void run();
    void handle_client(int client_fd);
    std::string read_http_request(int client_fd);
    void send_http_response(int client_fd, const std::string& response);
    std::string handle_request(const std::string& request, int client_fd);
    void parse_http_headers(std::istringstream& request_stream,
                          std::map<std::string, std::string>&     params);
    void parse_request_body(const std::string& request, 
                          std::map<std::string, std::string>& params);    
    void parse_json_body(const std::string& json_str, 
                       std::map<std::string, std::string>& params);
    std::map<std::string, std::string> parse_query_params(const std::string& query);
    std::string url_decode(const std::string& str);
    std::string get_mime_type(const std::string& path);
    
    // 添加CORS支持
    std::string build_cors_headers();
    
    // 错误响应生成
    std::string build_error_response(int status_code, const std::string& message);
    std::string build_success_response(const std::string& content, 
                                     const std::string& content_type = "application/json");
    
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