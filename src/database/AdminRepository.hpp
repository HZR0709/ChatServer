#pragma once
#include "IAdminRepository.hpp"
#include <openssl/sha.h>
#include <memory>
#include <sstream>
#include <iomanip>

class AdminRepository : public IAdminRepository {
public:
    explicit AdminRepository(std::shared_ptr<IDatabase> database);
    
    // IAdminRepository 接口实现
    bool create_admin_user(const std::string& username, const std::string& password_hash, const std::string& email = "") override;
    void create_default_admin() override;
    std::optional<AdminUser> get_admin_user(const std::string& username) override;
    std::optional<AdminUser> get_admin_user(int user_id) override;
    bool update_admin_last_login(int user_id) override;
    bool verify_admin_password(const std::string& username, const std::string& password) override;
    
    bool deactivate_admin_user(int user_id) override;
    bool activate_admin_user(int user_id) override;
    std::vector<AdminUser> get_all_admin_users() override;
    
private:
    std::shared_ptr<IDatabase> database_;
    
    std::string sha256(const std::string& str);
    AdminUser parse_admin_user(const std::vector<std::string>& row);
};