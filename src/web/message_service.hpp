#pragma once

#include "database/IAdminRepository.hpp" 
#include "service_result.hpp"
#include "database/IMessageRepository.hpp"
#include <memory>
#include <vector>

class MessageService {
private:
    std::shared_ptr<IMessageRepository> message_repo_;

public:
    explicit MessageService(std::shared_ptr<IMessageRepository> message_repo)
        : message_repo_(std::move(message_repo)) {}

    ServiceResult<bool> send_broadcast(const std::string& content, int from_user_id = -1) {
        if (content.empty()) {
            return ServiceResult<bool>::failure("消息内容不能为空", 400);
        }

        if (content.length() > 1000) {
            return ServiceResult<bool>::failure("消息内容过长", 400);
        }

        // 保存广播消息（系统消息类型为2）
        if (message_repo_->save_message(from_user_id, -1, 2, content)) {
            LOG_INFO("发送广播消息: " + content);
            return ServiceResult<bool>::success(true, "广播消息发送成功");
        } else {
            return ServiceResult<bool>::failure("广播消息保存失败", 500);
        }
    }

    ServiceResult<std::vector<MessageRecord>> export_messages(int limit = 1000) {
        if (limit <= 0 || limit > 5000) {
            return ServiceResult<std::vector<MessageRecord>>::failure("导出数量超出范围", 400);
        }

        auto messages = message_repo_->get_recent_messages(limit);
        return ServiceResult<std::vector<MessageRecord>>::success(messages, "消息导出成功");
    }

    ServiceResult<bool> clear_old_messages(int days_old = 30) {
        if (days_old < 1) {
            return ServiceResult<bool>::failure("天数必须大于0", 400);
        }

        int deleted_count = message_repo_->delete_old_messages(days_old);
        LOG_INFO("清理旧消息，删除数量: " + std::to_string(deleted_count));
        
        return ServiceResult<bool>::success(true, "清理了 " + std::to_string(deleted_count) + " 条旧消息");
    }

    ServiceResult<std::vector<MessageRecord>> get_recent_messages(int limit = 50) {
        if (limit <= 0 || limit > 1000) {
            limit = 50;
        }

        auto messages = message_repo_->get_recent_messages(limit);
        return ServiceResult<std::vector<MessageRecord>>::success(messages, "获取消息成功");
    }

    ServiceResult<int> get_total_messages_count() {
        int count = message_repo_->get_total_messages_count();
        return ServiceResult<int>::success(count, "获取消息总数成功");
    }
};