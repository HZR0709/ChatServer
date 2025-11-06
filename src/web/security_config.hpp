#pragma once
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "utils/logger.hpp"
#include "secret_generator.hpp"

struct SecurityConfig {
    std::string jwt_secret;
    int token_expiry_hours = 24;
    int min_password_length = 6;
    int max_concurrent_tokens = 5;
    bool enable_token_blacklist = true;
    
    // 加载默认配置（从环境变量或生成）
    static SecurityConfig load_default() {
        SecurityConfig config;
        
        // 首先尝试从环境变量读取
        const char* env_secret = std::getenv("JWT_SECRET");
        if (env_secret != nullptr && strlen(env_secret) > 0) {
            config.jwt_secret = env_secret;
            LOG_INFO("从环境变量加载 JWT 密钥");
        } else {
            // 生成临时密钥用于开发
            config.jwt_secret = SecretGenerator::generate_secure_secret(32);
            LOG_WARNING("未找到环境变量 JWT_SECRET，使用自动生成的密钥（仅用于开发）");
        }
        
        // 从环境变量读取其他配置
        const char* expiry = std::getenv("TOKEN_EXPIRY_HOURS");
        if (expiry != nullptr) {
            config.token_expiry_hours = std::stoi(expiry);
        }
        
        const char* min_pass = std::getenv("MIN_PASSWORD_LENGTH");
        if (min_pass != nullptr) {
            config.min_password_length = std::stoi(min_pass);
        }
        
        const char* max_tokens = std::getenv("MAX_CONCURRENT_TOKENS");
        if (max_tokens != nullptr) {
            config.max_concurrent_tokens = std::stoi(max_tokens);
        }
        
        const char* blacklist = std::getenv("ENABLE_TOKEN_BLACKLIST");
        if (blacklist != nullptr) {
            config.enable_token_blacklist = (std::string(blacklist) == "true");
        }
        
        return config;
    }
    
    // 从配置文件加载
    static SecurityConfig load_from_file(const std::string& filename = "config/security.json") {
        SecurityConfig config;
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开安全配置文件: " + filename);
        }
        
        nlohmann::json json_config;
        file >> json_config;
        
        config.jwt_secret = json_config.value("jwt_secret", "");
        config.token_expiry_hours = json_config.value("token_expiry_hours", 24);
        config.min_password_length = json_config.value("min_password_length", 6);
        config.max_concurrent_tokens = json_config.value("max_concurrent_tokens", 5);
        config.enable_token_blacklist = json_config.value("enable_token_blacklist", true);
        
        if (config.jwt_secret.empty()) {
            throw std::runtime_error("配置文件中未找到 jwt_secret");
        }
        
        return config;
    }
    
    // 验证配置
    bool validate() const {
        if (jwt_secret.empty() || jwt_secret.length() < 16) {
            return false;
        }
        if (token_expiry_hours <= 0 || token_expiry_hours > 24 * 30) {
            return false;
        }
        if (min_password_length < 4) {
            return false;
        }
        if (max_concurrent_tokens <= 0) {
            return false;
        }
        return true;
    }
    
    std::string to_string() const {
        return "SecurityConfig{secret_length=" + std::to_string(jwt_secret.length()) + 
               ", expiry_hours=" + std::to_string(token_expiry_hours) + 
               ", max_tokens=" + std::to_string(max_concurrent_tokens) + "}";
    }
};