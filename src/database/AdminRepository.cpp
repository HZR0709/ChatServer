#include "AdminRepository.hpp"
#include "utils/logger.hpp"

AdminRepository::AdminRepository(std::shared_ptr<IDatabase> database)
    : database_(std::move(database)) {}

bool AdminRepository::create_admin_user(const std::string& username, const std::string& password, const std::string& email) {
    if (!database_ || !database_->is_initialized()) {
        LOG_ERROR("数据库未初始化，无法创建管理员用户");
        return false;
    }
    
    // 对密码进行哈希
    std::string password_hash = sha256(password);
    
    const char* sql = "INSERT INTO admin_users (username, password_hash, email) VALUES (?, ?, ?);";
    std::vector<std::string> params = {username, password_hash, email};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_INFO("创建管理员用户: " + username);
    } else {
        LOG_ERROR("创建管理员用户失败: " + username);
    }
    return result;
}

void AdminRepository::create_default_admin() {
    // 检查是否已存在管理员
    auto admin = get_admin_user("admin");
    if (!admin.has_value()) {
        // 创建默认管理员账户
        create_admin_user("admin", "admin123", "admin@example.com");
        LOG_INFO("创建默认管理员账户: admin/admin123");
    }
}

std::optional<AdminUser> AdminRepository::get_admin_user(const std::string& username) {
    if (!database_ || !database_->is_initialized()) {
        return std::nullopt;
    }
    
    const char* sql = "SELECT id, username, email, created_at, last_login, is_active FROM admin_users WHERE username = ?;";
    std::vector<std::string> params = {username};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success || result.rows.empty()) {
        return std::nullopt;
    }
    
    return parse_admin_user(result.rows[0]);
}

std::optional<AdminUser> AdminRepository::get_admin_user(int admin_id) {
    AdminUser user;
    return user; 
}

bool AdminRepository::update_admin_last_login(int user_id) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE admin_users SET last_login = datetime('now') WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_DEBUG("更新管理员最后登录时间: ID=" + std::to_string(user_id));
    }
    return result;
}

bool AdminRepository::verify_admin_password(const std::string& username, const std::string& password) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    // 获取用户信息
    auto user = get_admin_user(username);
    if (!user.has_value()) {
        return false;
    }
    
    // 获取存储的密码哈希
    const char* sql = "SELECT password_hash FROM admin_users WHERE username = ?;";
    std::vector<std::string> params = {username};
    
    auto result = database_->execute_parameterized_query_with_result(sql, params);
    if (!result.success || result.rows.empty()) {
        return false;
    }
    
    std::string stored_hash = result.rows[0][0];
    
    // 验证密码
    std::string input_hash = sha256(password);
    return input_hash == stored_hash;
}

bool AdminRepository::deactivate_admin_user(int user_id) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE admin_users SET is_active = 0 WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_INFO("停用管理员用户: ID=" + std::to_string(user_id));
    } else {
        LOG_ERROR("停用管理员用户失败: ID=" + std::to_string(user_id));
    }
    return result;
}

bool AdminRepository::activate_admin_user(int user_id) {
    if (!database_ || !database_->is_initialized()) {
        return false;
    }
    
    const char* sql = "UPDATE admin_users SET is_active = 1 WHERE id = ?;";
    std::vector<std::string> params = {std::to_string(user_id)};
    
    bool result = database_->execute_parameterized_query(sql, params);
    if (result) {
        LOG_INFO("激活管理员用户: ID=" + std::to_string(user_id));
    } else {
        LOG_ERROR("激活管理员用户失败: ID=" + std::to_string(user_id));
    }
    return result;
}

std::vector<AdminUser> AdminRepository::get_all_admin_users() {
    std::vector<AdminUser> admins;
    
    if (!database_ || !database_->is_initialized()) {
        return admins;
    }
    
    const char* sql = "SELECT id, username, email, created_at, last_login, is_active FROM admin_users ORDER BY username;";
    auto result = database_->execute_query(sql);
    
    if (!result.success) {
        return admins;
    }
    
    for (const auto& row : result.rows) {
        admins.push_back(parse_admin_user(row));
    }
    
    return admins;
}

std::string AdminRepository::sha256(const std::string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

AdminUser AdminRepository::parse_admin_user(const std::vector<std::string>& row) {
    AdminUser user;
    
    if (row.size() >= 6) {
        user.id = std::stoi(row[0]);
        user.username = row[1];
        user.email = row[2];
        user.created_at = row[3];
        user.last_login = row[4];
        user.is_active = (row[5] == "1");
    }
    
    return user;
}