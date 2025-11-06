#pragma once
#include "IDatabase.hpp"
#include <optional>

class IAdminRepository {
public:
    virtual ~IAdminRepository() = default;
    
    virtual bool create_admin_user(const std::string& username, const std::string& password_hash, const std::string& email = "") = 0;
    virtual void create_default_admin() = 0;
    virtual std::optional<AdminUser> get_admin_user(const std::string& username) = 0;
    virtual std::optional<AdminUser> get_admin_user(int user_id) = 0;
    virtual bool update_admin_last_login(int user_id) = 0;
    virtual bool verify_admin_password(const std::string& username, const std::string& password) = 0;
    
    // 管理员管理
    virtual bool deactivate_admin_user(int user_id) = 0;
    virtual bool activate_admin_user(int user_id) = 0;
    virtual std::vector<AdminUser> get_all_admin_users() = 0;
};