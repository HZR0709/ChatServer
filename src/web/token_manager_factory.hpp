#pragma once
#include "token_manager.hpp"
#include "security_config.hpp"
#include "token_config.hpp"
#include <memory>

class TokenManagerFactory {
public:
    // 创建默认的 TokenManager
    static std::shared_ptr<ITokenManager> create_default() {
        SecurityConfig security_config = SecurityConfig::load_default();
        TokenConfig config{
            .secret_key = security_config.jwt_secret,
            .expiry_duration = std::chrono::hours(security_config.token_expiry_hours),
            .cleanup_interval_minutes = 60,
            .enable_blacklist = true,
            .max_concurrent_tokens_per_user = 5
        };
        
        return std::make_shared<TokenManager>(std::move(config));
    }
    
    // 根据配置创建 TokenManager
    static std::shared_ptr<ITokenManager> create_from_config(const TokenConfig& config) {
        if (!config.validate()) {
            throw std::invalid_argument("无效的Token配置");
        }
        return std::make_shared<TokenManager>(config);
    }
    
    // 创建用于测试的 TokenManager
    static std::shared_ptr<ITokenManager> create_for_test() {
        TokenConfig config{
            .secret_key = "test-secret-key-for-testing-only",
            .expiry_duration = std::chrono::hours(24),
            .cleanup_interval_minutes = 1, // 测试环境下快速清理
            .enable_blacklist = false,     // 测试环境下禁用黑名单
            .max_concurrent_tokens_per_user = 10
        };
        
        return std::make_shared<TokenManager>(std::move(config));
    }
    
    // 创建高安全级别的 TokenManager
    static std::shared_ptr<ITokenManager> create_high_security() {
        SecurityConfig security_config = SecurityConfig::load_default();
        TokenConfig config{
            .secret_key = security_config.jwt_secret,
            .expiry_duration = std::chrono::hours(4), // 短期令牌
            .cleanup_interval_minutes = 30,
            .enable_blacklist = true,
            .max_concurrent_tokens_per_user = 3 // 限制并发令牌数
        };
        
        return std::make_shared<TokenManager>(std::move(config));
    }
};