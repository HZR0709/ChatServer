#include "network/network_manager.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>

NetworkManager::NetworkManager() 
    : server_socket_(-1), is_initialized_(false) {
    memset(&server_addr_, 0, sizeof(server_addr_));
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize(int port) {
    // 创建socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置socket选项，避免地址占用
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        std::cerr << "设置socket选项失败: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // 绑定地址
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_addr.s_addr = INADDR_ANY;
    server_addr_.sin_port = htons(port);
    
    if (bind(server_socket_, (struct sockaddr*)&server_addr_, 
             sizeof(server_addr_)) < 0) {
        std::cerr << "绑定地址失败: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    // 开始监听
    if (listen(server_socket_, 10) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        close(server_socket_);
        return false;
    }
    
    is_initialized_ = true;
    std::cout << "服务器初始化成功，监听端口: " << port << std::endl;
    return true;
}

int NetworkManager::accept_connection() {
    if (!is_initialized_) {
        return -1;
    }
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_socket_, 
                          (struct sockaddr*)&client_addr, 
                          &client_len);
    
    if (client_fd < 0) {
        std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
        return -1;
    }
    
    // std::cout << "新客户端连接: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
    return client_fd;
}

bool NetworkManager::send_message(int fd, const std::string& msg) {
    // 发送消息长度
    uint32_t msg_len = htonl(msg.length());
    if (send(fd, &msg_len, sizeof(msg_len), 0) != sizeof(msg_len)) {
        return false;
    }
    
    // 发送消息内容
    if (send(fd, msg.c_str(), msg.length(), 0) != (ssize_t)msg.length()) {
        return false;
    }
    
    return true;
}

std::string NetworkManager::receive_message(int fd) {
    // 接收消息长度
    uint32_t msg_len_net;
    ssize_t bytes_received = recv(fd, &msg_len_net, sizeof(msg_len_net), 0);
    
    if (bytes_received <= 0) {
        return ""; // 连接关闭或错误
    }
    
    if (bytes_received != sizeof(msg_len_net)) {
        std::cerr << "接收消息长度不完整" << std::endl;
        return "";
    }
    
    uint32_t msg_len = ntohl(msg_len_net);
    
    // 接收消息内容
    std::string msg(msg_len, '\0');
    bytes_received = recv(fd, &msg[0], msg_len, 0);
    
    if (bytes_received != (ssize_t)msg_len) {
        std::cerr << "接收消息内容不完整" << std::endl;
        return "";
    }
    
    return msg;
}

void NetworkManager::close_connection(int fd) {
    close(fd);
}


void NetworkManager::shutdown() {
    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }
    is_initialized_ = false;
}