#include "MessageRepository.hpp"
#include "utils/logger.hpp"
#include <algorithm>

MessageRepository::MessageRepository(std::shared_ptr<IDatabase> database)
    : database_(std::move(database)) {}

bool MessageRepository::save_message(int from_user_id, int to_user_id, int message_type, const std::string& content) {
    if (!database_ || !database_->is_initialized()) {
        LOG_ERROR("数据库未初始化，无法保存消息");
        return false;
    }
    
    const char* sql = "INSERT INTO messages (from_user_id, to_user_id, message_type, content) VALUES (?, ?, ?, ?);";
    std::vector<std::string> params = {
        std::to_string(from_user_id),
        to_user_id == -1 ? "NULL" : std::to_string(to_user_id),
        std::to_string(message_type),
        content
    };
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("消息保存成功: 发送者=" + std::to_string(from_user_id) + 
                 ", 接收者=" + (to_user_id == -1 ? "广播" : std::to_string(to_user_id)));
    } else {
        LOG_ERROR("消息保存失败");
    }
    return result;
}

std::vector<MessageRecord> MessageRepository::get_recent_messages(int limit) {
    std::vector<MessageRecord> messages;
    
    if (!database_ || !database_->is_initialized()) {
        return messages;
    }
    
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "ORDER BY m.created_at DESC LIMIT ?;";
    
    std::vector<std::string> params = {std::to_string(limit)};
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    
    if (!result.success) {
        return messages;
    }
    
    for (const auto& row : result.rows) {
        messages.push_back(parse_message_record(row));
    }
    
    // 反转顺序，使最新的在最后
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<MessageRecord> MessageRepository::get_user_messages(int user_id, int limit) {
    std::vector<MessageRecord> messages;
    
    if (!database_ || !database_->is_initialized()) {
        return messages;
    }
    
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "WHERE m.from_user_id = ? OR m.to_user_id = ? "
        "ORDER BY m.created_at DESC LIMIT ?;";
    
    std::vector<std::string> params = {
        std::to_string(user_id),
        std::to_string(user_id), 
        std::to_string(limit)
    };
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    
    if (!result.success) {
        return messages;
    }
    
    for (const auto& row : result.rows) {
        messages.push_back(parse_message_record(row));
    }
    
    // 反转顺序，使最新的在最后
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<MessageRecord> MessageRepository::get_messages_since(int message_id) {
    std::vector<MessageRecord> messages;
    
    if (!database_ || !database_->is_initialized()) {
        return messages;
    }
    
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "WHERE m.id > ? "
        "ORDER BY m.created_at ASC;";
    
    std::vector<std::string> params = {std::to_string(message_id)};
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    
    if (!result.success) {
        return messages;
    }
    
    for (const auto& row : result.rows) {
        messages.push_back(parse_message_record(row));
    }
    
    return messages;
}

int MessageRepository::get_message_count() {
    if (!database_ || !database_->is_initialized()) {
        return 0;
    }
    
    const char* sql = "SELECT COUNT(*) FROM messages;";
    auto result = database_->execute_query(sql);
    
    if (!result.success || result.rows.empty()) {
        return 0;
    }
    
    return std::stoi(result.rows[0][0]);
}

int MessageRepository::get_total_messages_count() {
    return get_message_count();
}

int MessageRepository::delete_old_messages(int days_old) {
    // 待实现.............
    return days_old;
}

MessageRecord MessageRepository::parse_message_record(const std::vector<std::string>& row) {
    MessageRecord msg;
    
    if (row.size() >= 8) {
        msg.id = std::stoi(row[0]);
        msg.from_user_id = std::stoi(row[1]);
        
        // 处理可能的 NULL 值
        if (row[2].empty() || row[2] == "NULL") {
            msg.to_user_id = -1;
        } else {
            msg.to_user_id = std::stoi(row[2]);
        }
        
        msg.message_type = std::stoi(row[3]);
        msg.content = row[4];
        msg.created_at = row[5];
        msg.from_username = row[6];
        
        if (row.size() > 7 && !row[7].empty() && row[7] != "NULL") {
            msg.to_username = row[7];
        }
    }
    
    return msg;
}