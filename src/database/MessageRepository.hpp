#pragma once
#include "IMessageRepository.hpp"
#include <memory>

class MessageRepository : public IMessageRepository {
public:
    explicit MessageRepository(std::shared_ptr<IDatabase> database);
    
    // IMessageRepository 接口实现
    bool save_message(int from_user_id, int to_user_id, int message_type, const std::string& content) override;
    std::vector<MessageRecord> get_recent_messages(int limit = 50) override;
    std::vector<MessageRecord> get_user_messages(int user_id, int limit = 50) override;
    std::vector<MessageRecord> get_messages_since(int message_id) override;
    int get_message_count() override;
    int get_total_messages_count() override;
    int delete_old_messages(int days_old) override;
    
private:
    std::shared_ptr<IDatabase> database_;
    
    MessageRecord parse_message_record(const std::vector<std::string>& row);
};