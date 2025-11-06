#include "SessionRepository.hpp"
#include "utils/logger.hpp"

SessionRepository::SessionRepository(std::shared_ptr<IDatabase> database)
    : database_(std::move(database)) {}

bool SessionRepository::create_session(int user_id, int socket_fd, const std::string& ip_address) {
    if (!database_ || !database_->is_initialized()) {
        LOG_ERROR("数据库未初始化，无法创建会话");
        return false;
    }
    
    const char* sql = "INSERT INTO user_sessions (user_id, socket_fd, ip_address) VALUES (?, ?, ?);";
    std::vector<std::string> params = {
        std::to_string(user_id),
        std::to_string(socket_fd),
        ip_address
    };
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("会话创建成功: 用户ID=" + std::to_string(user_id) + 
                 ", Socket FD=" + std::to_string(socket_fd));
    } else {
        LOG_ERROR("会话创建失败");
    }
    return result;
}

bool SessionRepository::update_session_activity(int socket_fd) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE user_sessions SET last_activity = datetime('now') WHERE socket_fd = ?;";
    std::vector<std::string> params = {std::to_string(socket_fd)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("更新会话活动时间: Socket FD=" + std::to_string(socket_fd));
    }
    return result;
}

bool SessionRepository::delete_session(int socket_fd) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "DELETE FROM user_sessions WHERE socket_fd = ?;";
    std::vector<std::string> params = {std::to_string(socket_fd)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("删除会话: Socket FD=" + std::to_string(socket_fd));
    } else {
        LOG_ERROR("删除会话失败: Socket FD=" + std::to_string(socket_fd));
    }
    return result;
}

bool SessionRepository::delete_all_sessions() {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    bool result = database_->execute_sql("DELETE FROM user_sessions;");
    if (result) {
        LOG_INFO("删除所有会话记录");
    } else {
        LOG_ERROR("删除所有会话失败");
    }
    return result;
}

int SessionRepository::get_user_id_by_socket(int socket_fd) {
    if (!database_ || !database_->is_initialized()) {
        return -1;
    }
    
    const char* sql = "SELECT user_id FROM user_sessions WHERE socket_fd = ?;";
    std::vector<std::string> params = {std::to_string(socket_fd)};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success || result.rows.empty()) {
        return -1;
    }
    
    return std::stoi(result.rows[0][0]);
}

std::vector<int> SessionRepository::get_user_sessions(int user_id) {
    std::vector<int> sessions;
    
    if (!database_ || !database_->is_initialized()) {
        return sessions;
    }
    
    const char* sql = "SELECT socket_fd FROM user_sessions WHERE user_id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success) {
        return sessions;
    }
    
    for (const auto& row : result.rows) {
        sessions.push_back(std::stoi(row[0]));
    }
    
    return sessions;
}