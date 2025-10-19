#pragma once

#include <string>
#include <ctime>

// 用户信息结构
struct UserInfo {
    int user_id;
    std::string username;
    int socket_fd;
    std::string login_time;
    
    UserInfo(int id = -1, const std::string& name = "", int fd = -1) 
        : user_id(id), username(name), socket_fd(fd) {
        time_t now = time(nullptr);
        login_time = ctime(&now);
        // 移除换行符
        if (!login_time.empty() && login_time.back() == '\n') {
            login_time.pop_back();
        }
    }
};

// 消息结构
struct ChatMessage {
    int from_user;
    int to_user;  // -1表示广播消息
    std::string content;
    std::string timestamp;
    
    ChatMessage(int from = -1, int to = -1, const std::string& msg = "")
        : from_user(from), to_user(to), content(msg) {
        time_t now = time(nullptr);
        timestamp = ctime(&now);
        if (!timestamp.empty() && timestamp.back() == '\n') {
            timestamp.pop_back();
        }
    }
};

// 协议命令常量
namespace Protocol {
    const std::string LOGIN = "LOGIN";
    const std::string SEND = "SEND";
    const std::string BROADCAST = "BROADCAST";
    const std::string LIST = "LIST";
    const std::string QUIT = "QUIT";
    const std::string DELIMITER = "|";
}