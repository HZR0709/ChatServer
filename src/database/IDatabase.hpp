#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>

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

struct AdminUser {
    int id;
    std::string username;
    std::string email;
    std::string created_at;
    std::string last_login;
    bool is_active;
};

struct QueryResult {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> column_names;
    bool success;
};

class IDatabase {
public:
    virtual ~IDatabase() = default;
    
    // 基础操作
    virtual bool initialize(const std::string& db_path) = 0;
    virtual void shutdown() = 0;
    virtual bool execute_sql(const std::string& sql) = 0;
    virtual QueryResult execute_query(const std::string& sql) = 0;
    virtual bool execute_parameterized_query(const std::string& sql, 
                const std::vector<std::string>& params = {}) = 0;
    virtual QueryResult execute_parameterized_query_with_result(
                const std::string& sql, 
                const std::vector<std::string>& params = {}) = 0;
    
    
    // 连接状态
    virtual bool is_initialized() const = 0;
};