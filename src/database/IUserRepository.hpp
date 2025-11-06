#pragma once
#include "IDatabase.hpp"
#include <vector>
#include <optional>

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    
    virtual bool create_user(const std::string& username) = 0;
    virtual std::optional<UserRecord> get_user(const std::string& username) = 0;
    virtual std::optional<UserRecord> get_user(int user_id) = 0;
    virtual std::vector<UserRecord> get_all_users() = 0;
    virtual bool update_user_last_login(int user_id) = 0;
    virtual bool set_user_online_status(int user_id, bool online) = 0;
    
    // 统计信息
    virtual int get_total_users_count() = 0;
    virtual int get_online_users_count() = 0;
};