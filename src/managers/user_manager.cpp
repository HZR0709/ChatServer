#include "managers/user_manager.hpp"
#include <iostream>
#include <algorithm>

UserManager::UserManager() : next_user_id_(1) {}

UserManager::~UserManager() {
    std::unique_lock lock(users_mutex_);
    users_.clear();
    fd_to_user_id_.clear();
    name_to_user_id_.clear();
}

int UserManager::generate_user_id() {
    return next_user_id_++;
}

int UserManager::add_user(int socket_fd, const std::string& username) {
    std::unique_lock lock(users_mutex_);
    
    // 检查用户名是否已存在
    if (name_to_user_id_.find(username) != name_to_user_id_.end()) {
        return -1; // 用户名已存在
    }
    
    // 检查socket是否已注册
    if (fd_to_user_id_.find(socket_fd) != fd_to_user_id_.end()) {
        return -2; // socket已注册
    }
    
    int user_id = generate_user_id();
    UserInfo user_info(user_id, username, socket_fd);
    
    // 添加到所有映射
    users_[user_id] = user_info;
    fd_to_user_id_[socket_fd] = user_id;
    name_to_user_id_[username] = user_id;
    
    std::cout << "用户注册成功: " << username << " (ID: " << user_id 
              << ", FD: " << socket_fd << ")" << std::endl;
    
    return user_id;
}

bool UserManager::remove_user(int user_id) {
    std::unique_lock lock(users_mutex_);
    
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        return false;
    }
    
    const UserInfo& user_info = user_it->second;
    
    // 从所有映射中移除
    fd_to_user_id_.erase(user_info.socket_fd);
    name_to_user_id_.erase(user_info.username);
    users_.erase(user_it);
    
    std::cout << "用户移除成功: " << user_info.username 
              << " (ID: " << user_id << ")" << std::endl;
    
    return true;
}

bool UserManager::remove_user_by_fd(int socket_fd) {
    std::unique_lock lock(users_mutex_);
    
    auto fd_it = fd_to_user_id_.find(socket_fd);
    if (fd_it == fd_to_user_id_.end()) {
        return false;
    }
    
    int user_id = fd_it->second;
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        return false;
    }
    
    const UserInfo& user_info = user_it->second;
    
    // 从所有映射中移除
    name_to_user_id_.erase(user_info.username);
    users_.erase(user_it);
    fd_to_user_id_.erase(fd_it);
    
    std::cout << "用户移除成功: " << user_info.username 
              << " (ID: " << user_id << ", FD: " << socket_fd << ")" << std::endl;
    
    return true;
}

UserInfo* UserManager::find_user(int user_id) {
    std::shared_lock lock(users_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return nullptr;
    }
    
    return &(it->second);
}

UserInfo* UserManager::find_user_by_fd(int socket_fd) {
    std::shared_lock lock(users_mutex_);
    
    auto fd_it = fd_to_user_id_.find(socket_fd);
    if (fd_it == fd_to_user_id_.end()) {
        return nullptr;
    }
    
    auto user_it = users_.find(fd_it->second);
    if (user_it == users_.end()) {
        return nullptr;
    }
    
    return &(user_it->second);
}

UserInfo* UserManager::find_user_by_name(const std::string& username) {
    std::shared_lock lock(users_mutex_);
    
    auto name_it = name_to_user_id_.find(username);
    if (name_it == name_to_user_id_.end()) {
        return nullptr;
    }
    
    auto user_it = users_.find(name_it->second);
    if (user_it == users_.end()) {
        return nullptr;
    }
    
    return &(user_it->second);
}

std::vector<UserInfo> UserManager::get_online_users() {
    std::shared_lock lock(users_mutex_);
    
    std::vector<UserInfo> online_users;
    for (const auto& pair : users_) {
        online_users.push_back(pair.second);
    }
    
    return online_users;
}

int UserManager::get_user_count() {
    std::shared_lock lock(users_mutex_);
    return users_.size();
}

bool UserManager::set_user_username(int user_id, const std::string& username) {
    std::unique_lock lock(users_mutex_);
    
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        return false;
    }
    
    // 如果用户名已存在且不是当前用户
    auto name_it = name_to_user_id_.find(username);
    if (name_it != name_to_user_id_.end() && name_it->second != user_id) {
        return false;
    }
    
    // 更新用户名映射
    const std::string old_username = user_it->second.username;
    name_to_user_id_.erase(old_username);
    name_to_user_id_[username] = user_id;
    
    // 更新用户信息
    user_it->second.username = username;
    
    return true;
}

bool UserManager::is_user_online(int user_id) {
    std::shared_lock lock(users_mutex_);
    return users_.find(user_id) != users_.end();
}