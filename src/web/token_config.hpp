#pragma once
#include <chrono>
#include <string>
#include "security_config.hpp"

struct TokenConfig {
    std::string secret_key;
    std::chrono::hours expiry_duration{24};
    int cleanup_interval_minutes{60};
    bool enable_blacklist{true};
    int max_concurrent_tokens_per_user{5};
    
    // 从 SecurityConfig 创建
    static TokenConfig from_security_config(const SecurityConfig& security_config) {
        return TokenConfig{
            .secret_key = security_config.jwt_secret,
            .expiry_duration = std::chrono::hours(security_config.token_expiry_hours),
            .cleanup_interval_minutes = 60,
            .enable_blacklist = security_config.enable_token_blacklist,
            .max_concurrent_tokens_per_user = security_config.max_concurrent_tokens
        };
    }
    
    // 验证配置有效性
    bool validate() const {
        return !secret_key.empty() && 
               secret_key.length() >= 16 &&
               expiry_duration.count() > 0 &&
               expiry_duration.count() <= 24 * 30 && // 最多30天
               max_concurrent_tokens_per_user > 0;
    }
    
    std::string to_string() const {
        return "TokenConfig{secret_length=" + std::to_string(secret_key.length()) + 
               ", expiry_hours=" + std::to_string(expiry_duration.count()) + 
               ", max_tokens=" + std::to_string(max_concurrent_tokens_per_user) + "}";
    }
};