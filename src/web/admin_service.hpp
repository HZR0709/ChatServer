#pragma once
#include "database/IAdminRepository.hpp"
#include "service_result.hpp"
#include "token_manager.hpp"
#include "utils/logger.hpp"
#include "security_config.hpp"
#include <memory>

#define DEBUG

struct AdminLoginResult {
    AdminUser user;
    std::string token;
};

class AdminService {
private:
    std::shared_ptr<IAdminRepository> admin_repo_;
    std::shared_ptr<TokenManager> token_manager_;

public:
    AdminService(std::shared_ptr<IAdminRepository> admin_repo, 
                std::shared_ptr<TokenManager> token_manager)
        : admin_repo_(std::move(admin_repo))
        , token_manager_(std::move(token_manager)) {}

    // 登录并生成token（支持多设备）
    ServiceResult<AdminLoginResult> login(const std::string& username, const std::string& password) {
        // 参数验证
        if (username.empty() || password.empty()) {
            return ServiceResult<AdminLoginResult>::failure("用户名和密码不能为空", 400);
        }

        if (username.length() < 3) {
            return ServiceResult<AdminLoginResult>::failure("用户名至少需要3个字符", 400);
        }

        // 验证凭据
        if (!admin_repo_->verify_admin_password(username, password)) {
            LOG_WARNING("管理员登录失败: " + username);
            return ServiceResult<AdminLoginResult>::failure("用户名或密码错误", 401);
        }

        // 获取用户信息;  has_value()返回一个布尔值，指示对象是否存储了有效数据。
        auto user = admin_repo_->get_admin_user(username);
        if (!user.has_value()) {
            LOG_ERROR("获取管理员用户信息失败: " + username);
            return ServiceResult<AdminLoginResult>::failure("用户信息获取失败", 500);
        }

        // 更新最后登录时间
        if (!admin_repo_->update_admin_last_login(user->id)) {
            LOG_WARNING("更新最后登录时间失败: " + username);
            // 这里不返回错误，因为登录主要功能已经完成
        }

        // 生成令牌
        auto token_result = token_manager_->generate_token(user->id, username);
        if (!token_result.is_success()) {
            LOG_ERROR("令牌生成失败: " + username + ", 错误: " + token_result.message());
            return ServiceResult<AdminLoginResult>::failure("令牌生成失败", 500);
        }

        AdminLoginResult login_result;
        login_result.user = *user;
        login_result.token = token_result.unwrap();
        
        LOG_INFO("管理员登录成功: " + username);
        return ServiceResult<AdminLoginResult>::success(login_result, "登录成功");
    }

    ServiceResult<bool> register_admin(const std::string& username, const std::string& password, const std::string& email) {
        // 参数验证
        if (username.empty() || password.empty()) {
            return ServiceResult<bool>::failure("用户名和密码不能为空", 400);
        }

        if (username.length() < 3) {
            return ServiceResult<bool>::failure("用户名至少需要3个字符", 400);
        }

        if (password.length() < 6) {
            return ServiceResult<bool>::failure("密码至少需要6个字符", 400);
        }

        // 验证邮箱格式（如果提供了邮箱）
        if (!email.empty()) {
            // 简单的邮箱格式验证
            if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) {
                return ServiceResult<bool>::failure("邮箱格式不正确", 400);
            }
        }

        // 检查用户名是否已存在
        auto existing_user = admin_repo_->get_admin_user(username);
        if (existing_user.has_value()) {
            return ServiceResult<bool>::failure("用户名已存在", 409);
        }

        // 检查邮箱是否已被使用（如果提供了邮箱）
        if (!email.empty()) {
            // auto user_by_email = admin_repo_->get_admin_user_by_email(email);
            if (false) {    //暂时不考虑邮箱重复
                return ServiceResult<bool>::failure("邮箱已被使用", 409);
            }
        }

        // 创建用户
        if (admin_repo_->create_admin_user(username, password, email)) {
            LOG_INFO("创建新的管理员用户: " + username);
            return ServiceResult<bool>::success(true, "注册成功");
        } else {
            LOG_ERROR("创建管理员用户失败: " + username);
            return ServiceResult<bool>::failure("注册失败，请稍后重试", 500);
        }
    }

    ServiceResult<bool> kick_all_users() {
        return ServiceResult<bool>::success(true, "");
    }

    // 登出特定token
    ServiceResult<bool> logout(const std::string& token) {
        if (token.empty()) {
            return ServiceResult<bool>::failure("令牌不能为空", 400);
        }

        // 首先验证token，获取用户信息用于日志
        auto verify_result = token_manager_->verify_token(token);
        std::string username = "未知用户";
        if (verify_result.is_success()) {
            auto user = token_manager_->get_token_info(token);
            if (user.has_value()) {
                username = user->username;
            }
        }

        if (token_manager_->revoke_token(token)) {
            LOG_INFO("管理员登出: " + username);
            return ServiceResult<bool>::success(true, "退出成功");
        } else {
            LOG_WARNING("令牌撤销失败: " + username);
            return ServiceResult<bool>::failure("令牌撤销失败", 500);
        }
    }

    // 强制登出用户的所有设备
    ServiceResult<bool> logout_all_devices(int user_id) {
        if (user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        // 获取用户的所有token信息
        auto user_tokens = token_manager_->get_user_tokens(user_id);
        
        // 检查用户是否有活跃token（通过token列表是否为空来判断）
        if (user_tokens.empty()) {
            return ServiceResult<bool>::failure("用户没有活跃的登录会话", 404);
        }

        // 撤销用户的所有token
        token_manager_->revoke_all_user_tokens(user_id);
        
        // 从第一个token中获取用户名（假设所有token的用户名相同）
        std::string username = user_tokens[0].username;
        LOG_WARNING("强制登出用户所有设备: " + username);
        
        return ServiceResult<bool>::success(true, "所有设备已登出");
    }

    ServiceResult<AdminUser> verify_token(const std::string& token) {
        if (token.empty()) {
            return ServiceResult<AdminUser>::failure("令牌不能为空", 400);
        }

        auto result = token_manager_->verify_token(token);
        if (!result.is_success()) {
            return ServiceResult<AdminUser>::failure("令牌验证失败: " + result.message(), 401);
        }

        auto user_id = result.unwrap();
        auto user = admin_repo_->get_admin_user(user_id);
        if (!user.has_value()) {
            return ServiceResult<AdminUser>::failure("用户不存在", 404);
        }

        return ServiceResult<AdminUser>::success(*user, "令牌验证成功");
    }

    // 获取用户的所有活跃token（设备）
    ServiceResult<std::vector<TokenManager::TokenInfo>> get_user_sessions(int user_id) {
        if (user_id <= 0) {
            return ServiceResult<std::vector<TokenManager::TokenInfo>>::failure("无效的用户ID", 400);
        }

        // 验证用户存在
        auto user = admin_repo_->get_admin_user(user_id);
        if (!user.has_value()) {
            return ServiceResult<std::vector<TokenManager::TokenInfo>>::failure("用户不存在", 404);
        }

        try {
            auto tokens = token_manager_->get_user_tokens(user_id);
            return ServiceResult<std::vector<TokenManager::TokenInfo>>::success(tokens, "获取会话列表成功");
        } catch (const std::exception& e) {
            LOG_ERROR("获取用户会话失败, 用户ID: " + std::to_string(user_id) + ", 错误: " + e.what());
            return ServiceResult<std::vector<TokenManager::TokenInfo>>::failure(
                std::string("获取会话列表失败: ") + e.what(), 500);
        }
    }



    // 获取用户活跃设备数量
    ServiceResult<int> get_user_session_count(int user_id) {
        if (user_id <= 0) {
            return ServiceResult<int>::failure("无效的用户ID", 400);
        }

        try {
            int count = token_manager_->get_user_token_count(user_id);
            return ServiceResult<int>::success(count, "获取会话数量成功");
        } catch (const std::exception& e) {
            LOG_ERROR("获取用户会话数量失败, 用户ID: " + std::to_string(user_id) + ", 错误: " + e.what());
            return ServiceResult<int>::failure(std::string("获取会话数量失败: ") + e.what(), 500);
        }
    }
#ifndef DEBUG
    // 更新用户信息
    ServiceResult<AdminUser> update_user_info(int user_id, const std::string& email, const std::string& display_name) {
        if (user_id <= 0) {
            return ServiceResult<AdminUser>::failure("无效的用户ID", 400);
        }

        // 验证邮箱格式（如果提供了邮箱）
        if (!email.empty() && (email.find('@') == std::string::npos || email.find('.') == std::string::npos)) {
            return ServiceResult<AdminUser>::failure("邮箱格式不正确", 400);
        }

        // 检查邮箱是否已被其他用户使用
        if (!email.empty()) {
            auto existing_user = admin_repo_->get_admin_user_by_email(email);
            if (existing_user.has_value() && existing_user->id != user_id) {
                return ServiceResult<AdminUser>::failure("邮箱已被其他用户使用", 409);
            }
        }

        if (admin_repo_->update_admin_user(user_id, email, display_name)) {
            auto updated_user = admin_repo_->get_admin_user_by_id(user_id);
            if (updated_user.has_value()) {
                LOG_INFO("更新用户信息成功: 用户ID=" + std::to_string(user_id));
                return ServiceResult<AdminUser>::success(*updated_user, "用户信息更新成功");
            } else {
                return ServiceResult<AdminUser>::failure("获取更新后的用户信息失败", 500);
            }
        } else {
            LOG_ERROR("更新用户信息失败: 用户ID=" + std::to_string(user_id));
            return ServiceResult<AdminUser>::failure("用户信息更新失败", 500);
        }
    }

    // 修改密码
    ServiceResult<bool> change_password(int user_id, const std::string& old_password, const std::string& new_password) {
        if (user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        if (new_password.length() < 6) {
            return ServiceResult<bool>::failure("新密码至少需要6个字符", 400);
        }

        // 获取用户信息
        auto user = admin_repo_->get_admin_user_by_id(user_id);
        if (!user.has_value()) {
            return ServiceResult<bool>::failure("用户不存在", 404);
        }

        // 验证旧密码
        if (!admin_repo_->verify_admin_password(user->username, old_password)) {
            return ServiceResult<bool>::failure("原密码不正确", 401);
        }

        // 更新密码
        if (admin_repo_->update_admin_password(user_id, new_password)) {
            // 密码修改成功后，强制登出所有设备（安全考虑）
            token_manager_->revoke_all_user_tokens(user_id);
            
            LOG_INFO("用户修改密码成功: " + user->username);
            return ServiceResult<bool>::success(true, "密码修改成功，请重新登录");
        } else {
            LOG_ERROR("用户修改密码失败: " + user->username);
            return ServiceResult<bool>::failure("密码修改失败", 500);
        }
    }

    // 重置用户密码（管理员权限）
    ServiceResult<bool> reset_user_password(int admin_user_id, int target_user_id, const std::string& new_password) {
        if (admin_user_id <= 0 || target_user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        if (new_password.length() < 6) {
            return ServiceResult<bool>::failure("新密码至少需要6个字符", 400);
        }

        // 验证操作者权限（这里可以添加更复杂的权限检查）
        auto admin_user = admin_repo_->get_admin_user_by_id(admin_user_id);
        if (!admin_user.has_value()) {
            return ServiceResult<bool>::failure("操作者不存在", 404);
        }

        // 验证目标用户存在
        auto target_user = admin_repo_->get_admin_user_by_id(target_user_id);
        if (!target_user.has_value()) {
            return ServiceResult<bool>::failure("目标用户不存在", 404);
        }

        // 更新密码
        if (admin_repo_->update_admin_password(target_user_id, new_password)) {
            // 密码重置后，强制登出目标用户的所有设备
            token_manager_->revoke_all_user_tokens(target_user_id);
            
            LOG_WARNING("管理员重置用户密码: 操作者=" + admin_user->username + 
                       ", 目标用户=" + target_user->username);
            return ServiceResult<bool>::success(true, "密码重置成功");
        } else {
            LOG_ERROR("管理员重置用户密码失败: 操作者=" + admin_user->username + 
                     ", 目标用户=" + target_user->username);
            return ServiceResult<bool>::failure("密码重置失败", 500);
        }
    }

    // 获取用户列表（分页）
    ServiceResult<std::vector<AdminUser>> get_user_list(int page = 1, int page_size = 20) {
        if (page < 1) {
            page = 1;
        }
        if (page_size < 1 || page_size > 100) {
            page_size = 20;
        }

        try {
            int offset = (page - 1) * page_size;
            auto users = admin_repo_->get_admin_users(offset, page_size);
            return ServiceResult<std::vector<AdminUser>>::success(users, "获取用户列表成功");
        } catch (const std::exception& e) {
            LOG_ERROR("获取用户列表失败: " + std::string(e.what()));
            return ServiceResult<std::vector<AdminUser>>::failure(
                std::string("获取用户列表失败: ") + e.what(), 500);
        }
    }

    // 删除用户（管理员权限）
    ServiceResult<bool> delete_user(int admin_user_id, int target_user_id) {
        if (admin_user_id <= 0 || target_user_id <= 0) {
            return ServiceResult<bool>::failure("无效的用户ID", 400);
        }

        // 不能删除自己
        if (admin_user_id == target_user_id) {
            return ServiceResult<bool>::failure("不能删除自己的账户", 400);
        }

        // 验证操作者权限
        auto admin_user = admin_repo_->get_admin_user_by_id(admin_user_id);
        if (!admin_user.has_value()) {
            return ServiceResult<bool>::failure("操作者不存在", 404);
        }

        // 验证目标用户存在
        auto target_user = admin_repo_->get_admin_user_by_id(target_user_id);
        if (!target_user.has_value()) {
            return ServiceResult<bool>::failure("目标用户不存在", 404);
        }

        // 删除用户
        if (admin_repo_->delete_admin_user(target_user_id)) {
            // 删除用户后，撤销其所有token
            token_manager_->revoke_all_user_tokens(target_user_id);
            
            LOG_WARNING("管理员删除用户: 操作者=" + admin_user->username + 
                       ", 目标用户=" + target_user->username);
            return ServiceResult<bool>::success(true, "用户删除成功");
        } else {
            LOG_ERROR("管理员删除用户失败: 操作者=" + admin_user->username + 
                     ", 目标用户=" + target_user->username);
            return ServiceResult<bool>::failure("用户删除失败", 500);
        }
    }
#endif
};