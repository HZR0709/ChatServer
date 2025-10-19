#pragma once

#include <string>
#include <sqlite3.h>
#include <vector>
#include <memory>
#include "types/chat_types.hpp"

struct UserRecord {
    int id;
    std::string username;
    std::string created_at;
    std::string last_login;
    bool is_online;
};

struct MessageRecord {
    int id;
    int from_user_id;
    int to_user_id;
    int message_type;
    std::string content;
    std::string created_at;
    std::string from_username;
    std::string to_username;
};

class DatabaseManager {
public:
    static DatabaseManager& get_instance();
    
    bool initialize(const std::string& db_path);
    void shutdown();
    
    // 用户管理
    bool create_user(const std::string& username);
    UserRecord get_user(const std::string& username);
    UserRecord get_user(int user_id);
    std::vector<UserRecord> get_all_users();
    bool update_user_last_login(int user_id);
    bool set_user_online_status(int user_id, bool online);
    
    // 会话管理
    bool create_session(int user_id, int socket_fd, const std::string& ip_address = "");
    bool update_session_activity(int socket_fd);
    bool delete_session(int socket_fd);
    bool delete_all_sessions();
    
    // 消息管理
    bool save_message(int from_user_id, int to_user_id, int message_type, const std::string& content);
    std::vector<MessageRecord> get_recent_messages(int limit = 50);
    std::vector<MessageRecord> get_user_messages(int user_id, int limit = 50);
    std::vector<MessageRecord> get_messages_since(int message_id);
    int get_message_count();
    
    // 统计信息
    int get_total_users_count();
    int get_online_users_count();
    int get_total_messages_count();
    std::string get_database_size();
    
private:
    DatabaseManager();
    ~DatabaseManager();
    
    bool create_tables();
    bool execute_sql(const std::string& sql);
    
    sqlite3* db_;
    std::string db_path_;
    bool initialized_;
    
    // 防止拷贝
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};