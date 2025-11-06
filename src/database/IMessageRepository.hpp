#pragma once
#include "IDatabase.hpp"
#include <vector>
#include <memory>

class IMessageRepository {
public:
    virtual ~IMessageRepository() = default;
    
    virtual bool save_message(int from_user_id, int to_user_id, int message_type, const std::string& content) = 0;
    virtual std::vector<MessageRecord> get_recent_messages(int limit = 50) = 0;
    virtual std::vector<MessageRecord> get_user_messages(int user_id, int limit = 50) = 0;
    virtual std::vector<MessageRecord> get_messages_since(int message_id) = 0;
    virtual int get_message_count() = 0;
    virtual int delete_old_messages(int days_old) = 0;
    
    // 统计信息
    virtual int get_total_messages_count() = 0;
};