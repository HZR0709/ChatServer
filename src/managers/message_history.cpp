#include "managers/message_history.hpp"
#include <algorithm>

MessageHistory::MessageHistory(size_t max_size) 
    : max_size_(max_size) {}

MessageHistory::~MessageHistory() {
    clear();
}

void MessageHistory::add_message(const ChatMessage& message) {
    std::lock_guard lock(messages_mutex_);
    
    if (messages_.size() >= max_size_) {
        messages_.pop_front();
    }
    
    messages_.push_back(message);
}

std::vector<ChatMessage> MessageHistory::get_recent_messages(size_t count) const {
    std::lock_guard lock(messages_mutex_);
    
    std::vector<ChatMessage> result;
    if (messages_.empty()) {
        return result;
    }
    
    size_t start_idx = (messages_.size() > count) ? messages_.size() - count : 0;
    for (size_t i = start_idx; i < messages_.size(); ++i) {
        result.push_back(messages_[i]);
    }
    
    return result;
}

std::vector<ChatMessage> MessageHistory::get_messages_by_user(int user_id, size_t count) const {
    std::lock_guard lock(messages_mutex_);
    
    std::vector<ChatMessage> result;
    for (auto it = messages_.rbegin(); it != messages_.rend() && result.size() < count; ++it) {
        if (it->from_user == user_id) {
            result.push_back(*it);
        }
    }
    
    // 反转以保持时间顺序
    std::reverse(result.begin(), result.end());
    return result;
}

void MessageHistory::clear() {
    std::lock_guard lock(messages_mutex_);
    messages_.clear();
}

size_t MessageHistory::get_message_count() const {
    std::lock_guard lock(messages_mutex_);
    return messages_.size();
}