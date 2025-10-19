#pragma once 

#include <deque>
#include <string>
#include <mutex>
#include <ctime>
#include <vector>
#include "types/chat_types.hpp"

class MessageHistory {
public:
    MessageHistory(size_t max_size = 1000);
    ~MessageHistory();
    
    void add_message(const ChatMessage& message);
    std::vector<ChatMessage> get_recent_messages(size_t count = 50) const;
    std::vector<ChatMessage> get_messages_by_user(int user_id, size_t count = 50) const;
    void clear();
    size_t get_message_count() const;
    
private:
    std::deque<ChatMessage> messages_;
    mutable std::mutex messages_mutex_;
    size_t max_size_;
};