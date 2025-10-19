#include "web/web_server.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <unistd.h>
#include <sys/socket.h>
#include "utils/logger.hpp"
#include "database/database_manager.hpp"

WebServer::WebServer() 
    : running_(false), port_(8080), web_root_("./web") {}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start(int port) {
    if (running_) {
        return true;
    }
    
    port_ = port;
    
    if (!network_.initialize(port)) {
        LOG_ERROR("Web服务器初始化失败，端口: " + std::to_string(port));
        return false;
    }
    
    running_ = true;
    server_thread_ = std::thread(&WebServer::run, this);
    
    LOG_INFO("Web服务器启动成功，端口: " + std::to_string(port));
    return true;
}

void WebServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    network_.shutdown();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    LOG_INFO("Web服务器已停止");
}

void WebServer::run() {
    LOG_INFO("Web服务器进入主循环");
    
    while (running_) {
        int client_fd = network_.accept_connection();
        if (client_fd < 0) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        // 在新线程中处理HTTP请求
        std::thread client_thread(&WebServer::handle_client, this, client_fd);
        client_thread.detach();
    }
}

void WebServer::handle_client(int client_fd) {
    // 使用标准的HTTP协议读取请求
    std::string request = read_http_request(client_fd);
    
    if (!request.empty()) {
        std::string response = handle_request(request, client_fd);
        send_http_response(client_fd, response);
    }
    
    close(client_fd);
}

std::string WebServer::read_http_request(int client_fd) {
    std::string request;
    char buffer[4096];
    
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    while (true) {
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            break;
        }
        
        buffer[bytes_received] = '\0';
        request.append(buffer, bytes_received);
        
        // 检查是否接收到完整的HTTP请求（以空行结束）
        if (request.find("\r\n\r\n") != std::string::npos) {
            // 如果是POST请求，检查是否有Content-Length
            if (request.find("POST") == 0) {
                size_t content_length_pos = request.find("Content-Length: ");
                if (content_length_pos != std::string::npos) {
                    size_t content_length_end = request.find("\r\n", content_length_pos);
                    std::string content_length_str = request.substr(
                        content_length_pos + 16, content_length_end - content_length_pos - 16);
                    
                    try {
                        size_t content_length = std::stoul(content_length_str);
                        size_t body_start = request.find("\r\n\r\n") + 4;
                        if (request.length() - body_start < content_length) {
                            // 需要继续读取请求体
                            continue;
                        }
                    } catch (...) {
                        // 忽略解析错误
                    }
                }
            }
            break;
        }
    }
    
    return request;
}

void WebServer::send_http_response(int client_fd, const std::string& response) {
    send(client_fd, response.c_str(), response.length(), 0);
}

std::string WebServer::handle_request(const std::string& request, int client_fd) {
    std::istringstream request_stream(request);
    std::string method, path, version;
    request_stream >> method >> path >> version;
    
    // 解析查询参数
    size_t query_pos = path.find('?');
    std::string path_only = path;
    std::string query_string;
    
    if (query_pos != std::string::npos) {
        path_only = path.substr(0, query_pos);
        query_string = path.substr(query_pos + 1);
    }
    
    auto query_params = parse_query_params(query_string);
    
    // 添加客户端信息
    query_params["client_fd"] = std::to_string(client_fd);
    
    // 查找对应的处理器
    HttpHandler handler;
    
    if (method == "GET") {
        auto it = get_handlers_.find(path_only);
        if (it != get_handlers_.end()) {
            handler = it->second;
        }
    } else if (method == "POST") {
        auto it = post_handlers_.find(path_only);
        if (it != post_handlers_.end()) {
            handler = it->second;
        }
    }
    
    std::string response_body;
    std::string content_type = "text/html";
    
    if (handler) {
        response_body = handler(query_params);
        content_type = "application/json"; // API响应默认使用JSON
    } else {
        // 静态文件服务
        if (path_only == "/") {
            path_only = "/index.html";
        }
        
        std::string file_path = web_root_ + path_only;
        std::ifstream file(file_path, std::ios::binary);
        
        if (file) {
            response_body = std::string((std::istreambuf_iterator<char>(file)), 
                                      std::istreambuf_iterator<char>());
            content_type = get_mime_type(path_only);
        } else {
            // 404 Not Found
            response_body = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
            content_type = "text/html";
            
            std::string response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: " + content_type + "\r\n"
                "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + response_body;
            
            return response;
        }
    }
    
    // 构建HTTP响应
    std::string response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + std::to_string(response_body.length()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n" + response_body;
    
    return response;
}

void WebServer::get(const std::string& path, HttpHandler handler) {
    get_handlers_[path] = handler;
}

void WebServer::post(const std::string& path, HttpHandler handler) {
    post_handlers_[path] = handler;
}

std::map<std::string, std::string> WebServer::parse_query_params(const std::string& query) {
    std::map<std::string, std::string> params;
    
    if (query.empty()) {
        return params;
    }
    
    std::istringstream query_stream(query);
    std::string pair;
    
    while (std::getline(query_stream, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, pos));
            std::string value = url_decode(pair.substr(pos + 1));
            params[key] = value;
        } else {
            // 没有值的参数
            params[url_decode(pair)] = "";
        }
    }
    
    return params;
}

std::string WebServer::url_decode(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::string WebServer::get_mime_type(const std::string& path) {
    size_t dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string extension = path.substr(dot_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    static const std::map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"txt", "text/plain"}
    };
    
    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}