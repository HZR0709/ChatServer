#pragma once
#include "IUserRepository.hpp"
#include <memory>

class UserRepository : public IUserRepository {
public:
    explicit UserRepository(std::shared_ptr<IDatabase> database);
    
    // IUserRepository 接口实现
    bool create_user(const std::string& username) override;
    std::optional<UserRecord> get_user(const std::string& username) override;
    std::optional<UserRecord> get_user(int user_id) override;
    std::vector<UserRecord> get_all_users() override;
    bool update_user_last_login(int user_id) override;
    bool set_user_online_status(int user_id, bool online) override;
    
    int get_total_users_count() override;
    int get_online_users_count() override;
    
private:
    std::shared_ptr<IDatabase> database_;
};