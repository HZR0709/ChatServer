// models/message.hpp
#pragma once
#include <string>
#include <ctime>

struct Message {
    int id;
    int from_user_id;
    std::string from_username;
    int to_user_id;
    std::string to_username;
    int message_type;  // 1: 私聊, 2: 广播, 3: 系统消息
    std::string content;
    std::string created_at;
    bool is_read;
    
    // 默认构造函数
    Message() 
        : id(0)
        , from_user_id(-1)
        , to_user_id(-1)
        , message_type(1)
        , is_read(false) {
    }
    
    // 便捷构造函数
    Message(int from_uid, int to_uid, int type, const std::string& msg_content)
        : id(0)
        , from_user_id(from_uid)
        , to_user_id(to_uid)
        , message_type(type)
        , content(msg_content)
        , is_read(false) {
        // 设置创建时间
        set_current_time();
    }
    
    // 设置当前时间为创建时间
    void set_current_time() {
        std::time_t now = std::time(nullptr);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        created_at = time_str;
    }
    
    // 判断是否是广播消息
    bool is_broadcast() const {
        return message_type == 2;
    }
    
    // 判断是否是系统消息
    bool is_system_message() const {
        return message_type == 3;
    }
    
    // 判断是否是私聊消息
    bool is_private_message() const {
        return message_type == 1;
    }
    
    // 转换为JSON格式（如果需要）
    std::string to_json() const {
        // 简化实现，实际应该使用JSON库
        std::string json = "{";
        json += "\"id\":" + std::to_string(id) + ",";
        json += "\"from_user_id\":" + std::to_string(from_user_id) + ",";
        json += "\"from_username\":\"" + from_username + "\",";
        json += "\"to_user_id\":" + (to_user_id == -1 ? "null" : std::to_string(to_user_id)) + ",";
        json += "\"to_username\":\"" + (to_username.empty() ? "所有人" : to_username) + "\",";
        json += "\"message_type\":" + std::to_string(message_type) + ",";
        json += "\"content\":\"" + content + "\",";
        json += "\"created_at\":\"" + created_at + "\",";
        json += "\"is_read\":" + std::string(is_read ? "true" : "false");
        json += "}";
        return json;
    };
};