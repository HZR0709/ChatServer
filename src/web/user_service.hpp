#pragma once
#include "database/IUserRepository.hpp"
#include "service_result.hpp"
#include "utils/logger.hpp"
#include <memory>
#include <vector>

class UserService {
private:
    std::shared_ptr<IUserRepository> user_repo_;

public:
    explicit UserService(std::shared_ptr<IUserRepository> user_repo)
        : user_repo_(std::move(user_repo)) { }

    ServiceResult<std::vector<UserRecord>> get_all_users() {
        try {
            auto users = user_repo_->get_all_users();
            return ServiceResult<std::vector<UserRecord>>::success(users, "获取用户列表成功");
        } catch (const std::exception& e) {
            return ServiceResult<std::vector<UserRecord>>::failure("获取用户列表失败: " + std::string(e.what()), 500);
        }
    }

    ServiceResult<bool> kick_user(int user_id) {
        if (user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        LOG_INFO("踢出用户: " + std::to_string(user_id));
        // 实际实现应该断开用户连接
        return ServiceResult<bool>::success(true, "用户踢出成功");
    }

    ServiceResult<bool> kick_all_users() {
        LOG_WARNING("踢出所有用户");
        // 实际实现应该断开所有用户连接
        return ServiceResult<bool>::success(true, "所有用户踢出成功");
    }

    ServiceResult<bool> ban_user(int user_id, int hours = 24) {
        if (user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        if (hours <= 0) {
            return ServiceResult<bool>::failure("封禁时间必须大于0", 400);
        }

        LOG_INFO("封禁用户: " + std::to_string(user_id) + ", 时长: " + std::to_string(hours) + "小时");
        // 实际实现应该将用户加入黑名单
        return ServiceResult<bool>::success(true, "用户封禁成功");
    }
};