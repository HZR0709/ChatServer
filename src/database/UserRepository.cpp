#include "UserRepository.hpp"
#include "utils/logger.hpp"
#include <sstream>
#include <iomanip>

UserRepository::UserRepository(std::shared_ptr<IDatabase> database)
    : database_(std::move(database)) {}

bool UserRepository::create_user(const std::string& username) {
    if (!database_ || !database_->is_initialized()) {
        LOG_ERROR("数据库未初始化，无法创建用户");
        return false;
    }
    
    const char* sql = "INSERT OR IGNORE INTO users (username) VALUES (?);";
    std::vector<std::string> params = {username};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("用户创建成功: " + username);
    } else {
        LOG_ERROR("用户创建失败: " + username);
    }
    return result;
}

std::optional<UserRecord> UserRepository::get_user(const std::string& username) {
    if (!database_ || !database_->is_initialized()) {
        return std::nullopt;
    }
    
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users WHERE username = ?;";
    std::vector<std::string> params = {username};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success || result.rows.empty()) {
        return std::nullopt;
    }
    
    const auto& row = result.rows[0];
    UserRecord user;
    user.id = std::stoi(row[0]);
    user.username = row[1];
    user.created_at = row[2];
    user.last_login = row[3];
    user.is_online = (row[4] == "1");
    
    return user;
}

std::optional<UserRecord> UserRepository::get_user(int user_id) {
    if (!database_ || !database_->is_initialized()) {
        return std::nullopt;
    }
    
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success || result.rows.empty()) {
        return std::nullopt;
    }
    
    const auto& row = result.rows[0];
    UserRecord user;
    user.id = std::stoi(row[0]);
    user.username = row[1];
    user.created_at = row[2];
    user.last_login = row[3];
    user.is_online = (row[4] == "1");
    
    return user;
}

std::vector<UserRecord> UserRepository::get_all_users() {
    std::vector<UserRecord> users;
    
    if (!database_ || !database_->is_initialized()) {
        return users;
    }
    
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users ORDER BY username;";
    
    auto result = database_->execute_query(sql);
    if (!result.success) {
        return users;
    }
    
    for (const auto& row : result.rows) {
        UserRecord user;
        user.id = std::stoi(row[0]);
        user.username = row[1];
        user.created_at = row[2];
        user.last_login = row[3];
        user.is_online = (row[4] == "1");
        users.push_back(user);
    }
    
    return users;
}

bool UserRepository::update_user_last_login(int user_id) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE users SET last_login = datetime('now') WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("更新用户最后登录时间: " + std::to_string(user_id));
    } else {
        LOG_ERROR("更新用户最后登录时间失败: " + std::to_string(user_id));
    }
    return result;
}

bool UserRepository::set_user_online_status(int user_id, bool online) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE users SET is_online = ? WHERE id = ?;";
    std::vector<std::string> params = {
        online ? "1" : "0",
        std::to_string(user_id)
    };
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("更新用户在线状态: ID=" + std::to_string(user_id) + 
                 ", 状态=" + (online ? "在线" : "离线"));
    } else {
        LOG_ERROR("更新用户在线状态失败: " + std::to_string(user_id));
    }
    return result;
}

int UserRepository::get_total_users_count() {
    if (!database_ || !database_->is_initialized()) {
        return 0;
    }
    
    const char* sql = "SELECT COUNT(*) FROM users;";
    auto result = database_->execute_query(sql);
    
    if (!result.success || result.rows.empty()) {
        return 0;
    }
    
    return std::stoi(result.rows[0][0]);
}

int UserRepository::get_online_users_count() {
    if (!database_ || !database_->is_initialized()) {
        return 0;
    }
    
    const char* sql = "SELECT COUNT(*) FROM users WHERE is_online = 1;";
    auto result = database_->execute_query(sql);
    
    if (!result.success || result.rows.empty()) {
        return 0;
    }
    
    return std::stoi(result.rows[0][0]);
}