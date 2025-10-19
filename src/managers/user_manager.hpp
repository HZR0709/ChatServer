#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include "types/chat_types.hpp"

class UserManager {
public:
    UserManager();
    ~UserManager();
    
    // 用户管理
    int add_user(int socket_fd, const std::string& username);
    bool remove_user(int user_id);
    bool remove_user_by_fd(int socket_fd);
    UserInfo* find_user(int user_id);
    UserInfo* find_user_by_fd(int socket_fd);
    UserInfo* find_user_by_name(const std::string& username);
    std::vector<UserInfo> get_online_users();
    int get_user_count();
    
    // 用户状态
    bool set_user_username(int user_id, const std::string& username);
    bool is_user_online(int user_id);
    
private:
    std::unordered_map<int, UserInfo> users_;  // user_id -> UserInfo
    std::unordered_map<int, int> fd_to_user_id_; // socket_fd -> user_id
    std::unordered_map<std::string, int> name_to_user_id_; // username -> user_id
    
    mutable std::shared_mutex users_mutex_;
    int next_user_id_;
    
    int generate_user_id();
};