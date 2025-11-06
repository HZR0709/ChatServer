#include "web/web_server.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

WebServer::WebServer()
    : running_(false)
    , port_(8080)
    , web_root_("./web") { }

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

    running_       = true;
    server_thread_ = std::thread(&WebServer::run, this);

    LOG_INFO("Web服务器启动成功，端口: " + std::to_string(port) + ", 静态文件目录: " + web_root_);
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
    LOG_INFO("Web服务器进入主循环，已注册 " + std::to_string(get_handler_count()) + " 个路由处理器");

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
    } else {
        LOG_DEBUG("客户端连接已关闭或请求为空: FD " + std::to_string(client_fd));
    }

    close(client_fd);
}

std::string WebServer::read_http_request(int client_fd) {
    std::string request;
    char        buffer[4096];

    // 设置接收超时
    struct timeval tv;
    tv.tv_sec  = 5;
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
            if (request.find("POST") == 0 || request.find("PUT") == 0 || request.find("PATCH") == 0) {
                size_t content_length_pos = request.find("Content-Length: ");
                if (content_length_pos != std::string::npos) {
                    size_t      content_length_end = request.find("\r\n", content_length_pos);
                    std::string content_length_str = request.substr(
                        content_length_pos + 16, content_length_end - content_length_pos - 16);

                    try {
                        size_t content_length = std::stoul(content_length_str);
                        size_t body_start     = request.find("\r\n\r\n") + 4;
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
    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0) {
        LOG_ERROR("发送HTTP响应失败: FD " + std::to_string(client_fd));
    }
}

std::string WebServer::handle_request(const std::string& request, int client_fd) {
    DEBUG_HTTP(1, "=== 开始处理 HTTP 请求 ===");
    DEBUG_HTTP(2, "客户端 FD: " + std::to_string(client_fd));
    
    std::istringstream request_stream(request);
    std::string method, path, version;
    request_stream >> method >> path >> version;

    // 记录请求日志
    LOG_DEBUG("HTTP请求: " + method + " " + path + " " + version);
    DEBUG_HTTP(1, "请求行: " + method + " " + path + " " + version);
    
    // 打印原始请求用于调试（高级别调试）
    DEBUG_HTTP(3, "=== 原始请求开始 ===");
    DEBUG_HTTP(3, request);
    DEBUG_HTTP(3, "=== 原始请求结束 ===");

    // 解析查询参数
    size_t query_pos = path.find('?');
    std::string path_only = path;
    std::string query_string;
    
    if (query_pos != std::string::npos) {
        path_only = path.substr(0, query_pos);
        query_string = path.substr(query_pos + 1);
        DEBUG_HTTP(2, "查询字符串: " + query_string);
    } else {
        DEBUG_HTTP(2, "无查询字符串");
    }

    DEBUG_HTTP(2, "路径: " + path_only);
    
    auto query_params = parse_query_params(query_string);
    DEBUG_HTTP(2, "解析查询参数数量: " + std::to_string(query_params.size()));
    
    // 重新创建流用于解析头信息
    std::istringstream header_stream(request);
    std::string first_line;
    std::getline(header_stream, first_line); // 跳过请求行
    
    // 解析 HTTP 头信息
    parse_http_headers(header_stream, query_params);
    
    DEBUG_HTTP(2, "解析头信息后参数总数: " + std::to_string(query_params.size()));
    
    // 解析请求体参数（如果是POST/PUT等方法）
    if (method == "POST" || method == "PUT" || method == "PATCH") {
        DEBUG_HTTP(1, "解析 " + method + " 请求体");
        parse_request_body(request, query_params);
        DEBUG_HTTP(2, "解析请求体后参数总数: " + std::to_string(query_params.size()));
    }
    
    // 添加客户端信息
    query_params["client_fd"] = std::to_string(client_fd);
    
    // 打印所有参数进行调试（高级别调试）
    DEBUG_HTTP(3, "=== 所有参数列表开始 ===");
    for (const auto& [key, value] : query_params) {
        // 对敏感信息进行脱敏
        if (key.find("token") != std::string::npos || 
            key.find("auth") != std::string::npos ||
            key.find("password") != std::string::npos) {
            DEBUG_HTTP(3, key + " = [长度: " + std::to_string(value.length()) + "]");
        } else {
            DEBUG_HTTP(3, key + " = " + value);
        }
    }
    DEBUG_HTTP(3, "=== 所有参数列表结束 ===");
    
    std::string response_body;
    std::string content_type = "text/html";
    
    // 查找对应的处理器
    HttpHandler handler;
    bool handler_found = false;
    
    if (method == "GET") {
        auto it = get_handlers_.find(path_only);
        if (it != get_handlers_.end()) {
            handler = it->second;
            handler_found = true;
            DEBUG_API(1, "找到 GET 路由处理器: " + path_only);
        }
    } else if (method == "POST") {
        auto it = post_handlers_.find(path_only);
        if (it != post_handlers_.end()) {
            handler = it->second;
            handler_found = true;
            DEBUG_API(1, "找到 POST 路由处理器: " + path_only);
        }
    }
    
    if (handler_found) {
        DEBUG_API(1, "执行 API 处理器: " + path_only);
        try {
            response_body = handler(query_params);
            content_type = "application/json"; // API响应默认使用JSON
            DEBUG_API(1, "API 请求处理成功: " + path_only);
            DEBUG_API(2, "响应内容长度: " + std::to_string(response_body.length()));
        } catch (const std::exception& e) {
            LOG_ERROR("API请求处理异常: " + path_only + " - " + e.what());
            DEBUG_API(0, "API 请求处理异常: " + std::string(e.what()));
            return build_error_response(500, "服务器内部错误: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("API请求处理未知异常: " + path_only);
            DEBUG_API(0, "API 请求处理未知异常");
            return build_error_response(500, "服务器内部错误");
        }
    } else {
        DEBUG_HTTP(1, "未找到路由处理器，尝试静态文件服务: " + path_only);
        
        // 静态文件服务
        if (path_only == "/") {
            path_only = "/index.html";
            DEBUG_HTTP(2, "根路径重写为: " + path_only);
        }
        
        std::string file_path = web_root_ + path_only;
        DEBUG_HTTP(2, "静态文件路径: " + file_path);
        
        std::ifstream file(file_path, std::ios::binary);
        
        if (file) {
            response_body = std::string((std::istreambuf_iterator<char>(file)), 
                                      std::istreambuf_iterator<char>());
            content_type = get_mime_type(path_only);
            DEBUG_HTTP(1, "静态文件服务成功: " + file_path + " (大小: " + 
                      std::to_string(response_body.length()) + " 字节)");
            DEBUG_HTTP(2, "MIME 类型: " + content_type);
        } else {
            // 404 Not Found
            DEBUG_HTTP(0, "文件未找到: " + file_path);
            LOG_DEBUG("文件未找到: " + file_path);
            return build_error_response(404, "请求的资源未找到: " + path_only);
        }
    }
    
    // 构建响应
    std::string response = build_success_response(response_body, content_type);
    DEBUG_HTTP(1, "HTTP 响应构建完成，总大小: " + std::to_string(response.length()) + " 字节");
    DEBUG_HTTP(2, "响应状态: 200 OK, Content-Type: " + content_type);
    
    DEBUG_HTTP(1, "=== HTTP 请求处理完成 ===");
    
    return response;
}

void WebServer::get(const std::string& path, HttpHandler handler) {
    get_handlers_[path] = handler;
    LOG_DEBUG("注册GET路由: " + path);
}

void WebServer::post(const std::string& path, HttpHandler handler) {
    post_handlers_[path] = handler;
    LOG_DEBUG("注册POST路由: " + path);
}

/**
 * @brief 解析 HTTP 头信息
 * @param request_stream 请求流
 * @param params 参数映射，用于存储解析出的头信息
 */
void WebServer::parse_http_headers(std::istringstream& header_stream, 
                                  std::map<std::string, std::string>& params) {
    std::string line;
    
    DEBUG_HTTP(1, "=== 开始解析 HTTP 头信息 ===");
    
    // 首先跳过请求行（第一行）
    std::getline(header_stream, line);
    DEBUG_HTTP(2, "跳过请求行: '" + line + "'");
    
    // 读取头信息
    while (std::getline(header_stream, line)) {
        // 去除回车符
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        DEBUG_HTTP(3, "原始行: '" + line + "'");
        
        // 空行表示头信息解析结束
        if (line.empty()) {
            DEBUG_HTTP(2, "遇到空行，头信息解析结束");
            break;
        }
        
        // 解析头字段
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = line.substr(0, colon_pos);
            std::string header_value = line.substr(colon_pos + 1);
            
            // 去除首尾空格
            header_name.erase(0, header_name.find_first_not_of(" \t"));
            header_name.erase(header_name.find_last_not_of(" \t") + 1);
            header_value.erase(0, header_value.find_first_not_of(" \t"));
            header_value.erase(header_value.find_last_not_of(" \t") + 1);
            
            DEBUG_HTTP(2, "解析到头字段: '" + header_name + "' = '" + header_value + "'");
            
            // 将头名称转换为小写，便于统一处理
            std::transform(header_name.begin(), header_name.end(), 
                         header_name.begin(), ::tolower);
            
            // 存储头信息
            params["header_" + header_name] = header_value;
            DEBUG_HTTP(3, "存储为参数: 'header_" + header_name + "' = '" + header_value + "'");
            
            // 特别处理 Authorization 头
            if (header_name == "authorization") {
                DEBUG_AUTH(1, "检测到 Authorization 头，值: '" + header_value + "'");
                
                // 提取 Bearer token
                if (header_value.find("Bearer ") == 0) {
                    std::string token = header_value.substr(7);
                    params["token"] = token;
                    DEBUG_AUTH(1, "提取 Bearer token: '" + token + "'");
                }
            }
        } else {
            DEBUG_HTTP(1, "无法解析的行（缺少冒号）: '" + line + "'");
        }
    }
    
    DEBUG_HTTP(1, "=== HTTP 头信息解析完成 ===");
    DEBUG_HTTP(1, "总共解析出 " + std::to_string(params.size()) + " 个参数");
}

void WebServer::parse_request_body(const std::string& request,
    std::map<std::string, std::string>&               params) {
    // 找到请求头结束位置
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return;
    }

    // 提取请求体
    size_t      body_start = header_end + 4; // 跳过 \r\n\r\n
    std::string body       = request.substr(body_start);

    if (body.empty()) {
        return;
    }

    // 获取 Content-Type
    std::string content_type;
    size_t      content_type_pos = request.find("Content-Type: ");
    if (content_type_pos != std::string::npos && content_type_pos < header_end) {
        size_t content_type_end = request.find("\r\n", content_type_pos);
        if (content_type_end != std::string::npos) {
            content_type = request.substr(content_type_pos + 14,
                content_type_end - content_type_pos - 14);
            // 去除空格
            content_type.erase(0, content_type.find_first_not_of(" \t"));
            content_type.erase(content_type.find_last_not_of(" \t") + 1);
        }
    }

    // 根据 Content-Type 解析请求体
    if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
        // 表单数据: key=value&key2=value2
        auto body_params = parse_query_params(body);
        params.insert(body_params.begin(), body_params.end());

    } else if (content_type.find("application/json") != std::string::npos) {
        // JSON数据: {"key": "value", "key2": "value2"}
        parse_json_body(body, params);

    } else if (content_type.find("multipart/form-data") != std::string::npos) {
        // 文件上传
        params["_raw_body"] = body;

    } else {
        // 其他类型
        params["_raw_body"] = body;
    }
}

void WebServer::parse_json_body(const std::string& json_str,
    std::map<std::string, std::string>&            params) {
    try {
        // 使用 ModernJsonParser 解析 JSON
        auto result = ModernJsonParser::parse(json_str);

        if (!result) {
            params["_json_body"] = json_str;
            return;
        }

        const auto& json_obj = result.value();

        for (const auto& [key, value] : json_obj) {
            // 将 JSON 值转换为字符串
            std::string value_str = ModernJsonParser::value_to_string(value);
            // 添加到参数表
            params[key] = value_str;
        }

    } catch (const std::exception& e) {
        LOG_WARNING("JSON解析失败: " + std::string(e.what()));
        params["_json_body"] = json_str;
    } catch (...) {
        LOG_WARNING("JSON解析未知错误");
        params["_json_body"] = json_str;
    }
}

std::map<std::string, std::string> WebServer::parse_query_params(const std::string& query) {
    std::map<std::string, std::string> params;

    if (query.empty()) {
        return params;
    }

    std::istringstream query_stream(query);
    std::string        pair;

    while (std::getline(query_stream, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key   = url_decode(pair.substr(0, pos));
            std::string value = url_decode(pair.substr(pos + 1));
            params[key]       = value;
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
            int                value;
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
        { "html", "text/html" },
        { "htm", "text/html" },
        { "css", "text/css" },
        { "js", "application/javascript" },
        { "json", "application/json" },
        { "png", "image/png" },
        { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },
        { "gif", "image/gif" },
        { "svg", "image/svg+xml" },
        { "ico", "image/x-icon" },
        { "txt", "text/plain" },
        { "xml", "application/xml" },
        { "pdf", "application/pdf" },
        { "zip", "application/zip" },
        { "woff", "font/woff" },
        { "woff2", "font/woff2" },
        { "ttf", "font/ttf" }
    };

    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

std::string WebServer::build_cors_headers() {
    return "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
           "Access-Control-Max-Age: 86400\r\n";
}

std::string WebServer::build_error_response(int status_code, const std::string& message) {
    std::string status_text;
    switch (status_code) {
    case 400:
        status_text = "Bad Request";
        break;
    case 404:
        status_text = "Not Found";
        break;
    case 500:
        status_text = "Internal Server Error";
        break;
    default:
        status_text = "Error";
        break;
    }

    std::stringstream json;
    json << "{"
         << "\"success\": false,"
         << "\"message\": \"" << message << "\","
         << "\"error\": \"" << status_text << "\""
         << "}";

    std::string response_body = json.str();

    std::stringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << response_body.length() << "\r\n"
             << build_cors_headers()
             << "Connection: close\r\n"
             << "\r\n"
             << response_body;

    return response.str();
}

std::string WebServer::build_success_response(const std::string& content,
    const std::string&                                           content_type) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << content.length() << "\r\n"
             << build_cors_headers()
             << "Connection: close\r\n"
             << "\r\n"
             << content;

    return response.str();
}