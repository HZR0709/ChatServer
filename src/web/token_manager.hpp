#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <set>
#include <random>
#include <shared_mutex>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>
#include "token_config.hpp"
#include "service_result.hpp"
#include "utils/logger.hpp"

class ITokenManager {
public:
    struct TokenInfo {
        int user_id;
        std::string username;
        std::time_t created_at;
        std::time_t expires_at;
        std::string token_hash; // 存储token的哈希，用于验证
        
        // 检查是否过期
        bool is_expired() const {
            return std::time(nullptr) > expires_at;
        }
        
        // 转换为JSON（用于API返回）
        nlohmann::json to_json() const {
            return {
                {"user_id", user_id},
                {"username", username},
                {"created_at", created_at},
                {"expires_at", expires_at}
            };
        }
    };

    virtual ~ITokenManager() = default;
    
    virtual ServiceResult<std::string> generate_token(int user_id, const std::string& username, 
                                                     const std::string& device_info = "") = 0;
    virtual ServiceResult<int> verify_token(const std::string& token) = 0;
    virtual bool revoke_token(const std::string& token) = 0;
    virtual void revoke_all_user_tokens(int user_id) = 0;
    virtual void cleanup_expired_tokens() = 0;
    virtual std::optional<TokenInfo> get_token_info(const std::string& token) = 0;
    virtual std::vector<TokenInfo> get_user_tokens(int user_id) = 0;
};

class TokenManager : public ITokenManager {
private:
    TokenConfig config_;
    
    // 主要存储：token -> TokenInfo
    std::unordered_map<std::string, TokenInfo> token_store_;
    
    // 索引：user_id -> set of tokens（用于快速查找用户的所有token）
    std::unordered_map<int, std::set<std::string>> user_tokens_index_;
    
    // 黑名单
    std::unordered_map<std::string, std::time_t> blacklist_;
    
    // 读写锁保护数据
    mutable std::shared_mutex store_mutex_;
    mutable std::shared_mutex blacklist_mutex_;

    // 生成token哈希（用于验证）
    std::string generate_token_hash(const std::string& token) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len;
        
        HMAC(EVP_sha256(), 
             config_.secret_key.c_str(), config_.secret_key.length(),
             reinterpret_cast<const unsigned char*>(token.c_str()),
             token.length(), digest, &digest_len);
        
        std::stringstream ss;
        for (unsigned int i = 0; i < digest_len; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(digest[i]);
        }
        return ss.str();
    }

    // Base64编码（简化实现）
    std::string base64_encode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
            
        std::string output;
        int val = 0, valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) {
            output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        
        while (output.size() % 4) {
            output.push_back('=');
        }
        
        return output;
    }

    std::string base64_decode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
            
        std::string output;
        std::vector<int> T(256, -1);
        
        for (int i = 0; i < 64; i++) {
            T[base64_chars[i]] = i;
        }
        
        int val = 0, valb = -8;
        for (unsigned char c : input) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                output.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        
        return output;
    }

    // 生成安全的token
    std::string generate_secure_token(const TokenInfo& info) {
        // 创建唯一标识
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        std::vector<unsigned char> random_bytes(32);
        for (int i = 0; i < 32; ++i) {
            random_bytes[i] = dis(gen);
        }
        
        // 创建token载荷
        std::stringstream payload;
        payload << info.user_id << ":"
                << info.username << ":"
                << info.created_at << ":"
                << info.expires_at << ":"
                << std::to_string(dis(gen));
        
        std::string payload_str = payload.str();
        std::string payload_b64 = base64_encode(payload_str);
        
        // 生成签名
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len;
        
        HMAC(EVP_sha256(), 
             config_.secret_key.c_str(), config_.secret_key.length(),
             reinterpret_cast<const unsigned char*>(payload_b64.c_str()),
             payload_b64.length(), digest, &digest_len);
        
        std::string signature_b64 = base64_encode(
            std::string(reinterpret_cast<char*>(digest), digest_len));
        
        // 组合token: header.payload.signature
        return "v1." + payload_b64 + "." + signature_b64;
    }

    // 验证token签名
    bool verify_token_signature(const std::string& token) {
        // 解析token格式: v1.payload.signature
        if (token.substr(0, 3) != "v1.") {
            return false;
        }
        
        size_t first_dot = token.find('.', 3);
        if (first_dot == std::string::npos) {
            return false;
        }
        
        std::string payload_b64 = token.substr(3, first_dot - 3);
        std::string signature_b64 = token.substr(first_dot + 1);
        
        // 重新计算签名
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len;
        
        HMAC(EVP_sha256(),
             config_.secret_key.c_str(), config_.secret_key.length(),
             reinterpret_cast<const unsigned char*>(payload_b64.c_str()),
             payload_b64.length(), digest, &digest_len);
        
        std::string computed_signature_b64 = base64_encode(
            std::string(reinterpret_cast<char*>(digest), digest_len));
        
        return signature_b64 == computed_signature_b64;
    }

    // 解析token载荷
    TokenInfo parse_token_payload(const std::string& token) {
        if (token.substr(0, 3) != "v1.") {
            throw std::invalid_argument("不支持的token版本");
        }
        
        size_t first_dot = token.find('.', 3);
        if (first_dot == std::string::npos) {
            throw std::invalid_argument("无效的token格式");
        }
        
        std::string payload_b64 = token.substr(3, first_dot - 3);
        std::string payload = base64_decode(payload_b64);
        
        std::stringstream ss(payload);
        std::string part;
        TokenInfo info;
        
        std::getline(ss, part, ':');
        info.user_id = std::stoi(part);
        
        std::getline(ss, info.username, ':');

        std::getline(ss, part, ':');
        info.created_at = std::stoll(part);
        
        std::getline(ss, part, ':');
        info.expires_at = std::stoll(part);
        
        // 忽略随机部分
        
        return info;
    }

    // 检查黑名单
    bool is_token_blacklisted(const std::string& token) {
        std::shared_lock lock(blacklist_mutex_);
        auto it = blacklist_.find(token);
        if (it != blacklist_.end()) {
            // 检查黑名单条目是否过期
            if (std::time(nullptr) > it->second) {
                std::unique_lock write_lock(blacklist_mutex_);
                blacklist_.erase(it);
                return false;
            }
            return true;
        }
        return false;
    }

    // 强制令牌限制
    void enforce_token_limits(int user_id) {
        if (config_.max_concurrent_tokens_per_user <= 0) return;
        
        std::unique_lock lock(store_mutex_);
        auto user_tokens_it = user_tokens_index_.find(user_id);
        if (user_tokens_it == user_tokens_index_.end()) {
            return;
        }
        
        int token_count = user_tokens_it->second.size();
        if (token_count >= config_.max_concurrent_tokens_per_user) {
            // 找到该用户最早的令牌并移除
            std::string oldest_token;
            std::time_t oldest_time = std::numeric_limits<std::time_t>::max();
            
            for (const auto& token : user_tokens_it->second) {
                auto token_it = token_store_.find(token);
                if (token_it != token_store_.end() && 
                    token_it->second.created_at < oldest_time) {
                    oldest_time = token_it->second.created_at;
                    oldest_token = token;
                }
            }
            
            if (!oldest_token.empty()) {
                revoke_token_internal(oldest_token);
                LOG_INFO("达到令牌限制，移除用户 " + std::to_string(user_id) + " 的旧令牌");
            }
        }
    }

    // 内部撤销token（不获取锁）
    void revoke_token_internal(const std::string& token) {
        auto token_it = token_store_.find(token);
        if (token_it != token_store_.end()) {
            int user_id = token_it->second.user_id;
            
            // 从主存储移除
            token_store_.erase(token_it);
            
            // 从用户索引移除
            auto user_tokens_it = user_tokens_index_.find(user_id);
            if (user_tokens_it != user_tokens_index_.end()) {
                user_tokens_it->second.erase(token);
                if (user_tokens_it->second.empty()) {
                    user_tokens_index_.erase(user_tokens_it);
                }
            }
        }
    }

public:
    explicit TokenManager(TokenConfig config) : config_(std::move(config)) {
        if (!config_.validate()) {
            throw std::invalid_argument("无效的Token配置");
        }
        LOG_INFO("TokenManager 初始化: " + config_.to_string());
    }

    ServiceResult<std::string> generate_token(int user_id, const std::string& username, 
                                            const std::string& device_info = "") override {
        // 强制令牌限制
        enforce_token_limits(user_id);
        
        try {
            TokenInfo info;
            info.user_id = user_id;
            info.username = username;
            info.created_at = std::time(nullptr);
            info.expires_at = info.created_at + 
                            std::chrono::duration_cast<std::chrono::seconds>(
                                config_.expiry_duration).count();
            
            std::string token = generate_secure_token(info);
            info.token_hash = generate_token_hash(token);
            
            // 存储token
            std::unique_lock lock(store_mutex_);
            token_store_[token] = info;
            user_tokens_index_[user_id].insert(token);
            
            LOG_INFO("生成令牌: 用户=" + username + "(" + std::to_string(user_id) + 
                    "), 设备=" + device_info + ", 令牌数=" + 
                    std::to_string(user_tokens_index_[user_id].size()));
            return ServiceResult<std::string>::success(token, "令牌生成成功");
        } catch (const std::exception& e) {
            return ServiceResult<std::string>::failure(std::string("令牌生成失败: ") + e.what(), 500);
        }
    }

    ServiceResult<int> verify_token(const std::string& token) override {
        // 检查黑名单
        if (config_.enable_blacklist && is_token_blacklisted(token)) {
            return ServiceResult<int>::failure("令牌已被撤销", 401);
        }
        
        // 检查内存中的有效令牌
        {
            std::shared_lock lock(store_mutex_);
            auto it = token_store_.find(token);
            if (it != token_store_.end()) {
                if (it->second.is_expired()) {
                    return ServiceResult<int>::failure("令牌已过期", 401);
                }
                return ServiceResult<int>::success(it->second.user_id, "令牌验证成功");
            }
        }
        
        // 验证令牌签名和解析
        try {
            if (!verify_token_signature(token)) {
                return ServiceResult<int>::failure("无效的令牌签名", 401);
            }
            
            TokenInfo info = parse_token_payload(token);
            
            if (info.is_expired()) {
                return ServiceResult<int>::failure("令牌已过期", 401);
            }
            
            return ServiceResult<int>::success(info.user_id, "令牌验证成功");
        } catch (const std::exception& e) {
            return ServiceResult<int>::failure(std::string("令牌验证失败: ") + e.what(), 401);
        }
    }

    bool revoke_token(const std::string& token) override {
        std::unique_lock lock(store_mutex_);
        
        bool was_valid = token_store_.find(token) != token_store_.end();
        if (was_valid) {
            revoke_token_internal(token);
            
            // 添加到黑名单（如果启用）
            if (config_.enable_blacklist) {
                std::unique_lock blacklist_lock(blacklist_mutex_);
                blacklist_[token] = std::time(nullptr) + 3600; // 黑名单保留1小时
            }
            
            LOG_INFO("令牌已撤销: " + token.substr(0, 16) + "...");
        }
        
        return was_valid;
    }

    void revoke_all_user_tokens(int user_id) override {
        std::unique_lock lock(store_mutex_);
        
        auto user_tokens_it = user_tokens_index_.find(user_id);
        if (user_tokens_it == user_tokens_index_.end()) {
            return;
        }
        
        // 复制token集合，因为我们要修改原集合
        std::set<std::string> tokens_to_remove = user_tokens_it->second;
        
        for (const auto& token : tokens_to_remove) {
            revoke_token_internal(token);
            
            if (config_.enable_blacklist) {
                std::unique_lock blacklist_lock(blacklist_mutex_);
                blacklist_[token] = std::time(nullptr) + 3600;
            }
        }
        
        LOG_INFO("撤销用户 " + std::to_string(user_id) + " 的所有令牌: " + 
                std::to_string(tokens_to_remove.size()) + " 个令牌");
    }

    void cleanup_expired_tokens() override {
        std::time_t now = std::time(nullptr);
        std::vector<std::string> expired_tokens;
        
        {
            std::unique_lock lock(store_mutex_);
            for (const auto& [token, info] : token_store_) {
                if (info.is_expired()) {
                    expired_tokens.push_back(token);
                }
            }
            
            for (const auto& token : expired_tokens) {
                revoke_token_internal(token);
            }
        }
        
        // 清理过期黑名单
        {
            std::unique_lock lock(blacklist_mutex_);
            std::vector<std::string> expired_blacklist;
            for (const auto& [token, expiry] : blacklist_) {
                if (now > expiry) {
                    expired_blacklist.push_back(token);
                }
            }
            for (const auto& token : expired_blacklist) {
                blacklist_.erase(token);
            }
        }
        
        if (!expired_tokens.empty()) {
            LOG_DEBUG("清理了 " + std::to_string(expired_tokens.size()) + " 个过期令牌");
        }
    }
    
    std::optional<TokenInfo> get_token_info(const std::string& token) override {
        std::shared_lock lock(store_mutex_);
        auto it = token_store_.find(token);
        if (it != token_store_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<TokenInfo> get_user_tokens(int user_id) override {
        std::vector<TokenInfo> tokens;
        std::shared_lock lock(store_mutex_);
        
        auto user_tokens_it = user_tokens_index_.find(user_id);
        if (user_tokens_it != user_tokens_index_.end()) {
            for (const auto& token : user_tokens_it->second) {
                auto token_it = token_store_.find(token);
                if (token_it != token_store_.end()) {
                    tokens.push_back(token_it->second);
                }
            }
        }
        
        return tokens;
    }
    
    // 获取活跃令牌数量
    size_t get_active_token_count() const {
        std::shared_lock lock(store_mutex_);
        return token_store_.size();
    }

    // 获取活跃用户数量
    size_t get_active_user_count() const {
        std::shared_lock lock(store_mutex_);
        return user_tokens_index_.size();
    }

    // 获取特定用户的令牌数量
    size_t get_user_token_count(int user_id) const {
        std::shared_lock lock(store_mutex_);
        auto it = user_tokens_index_.find(user_id);
        return it != user_tokens_index_.end() ? it->second.size() : 0;
    }

    // 获取用户活跃令牌数量
    int get_user_token_count(int user_id) {
        std::shared_lock lock(store_mutex_);
        auto it = user_tokens_index_.find(user_id);
        return it != user_tokens_index_.end() ? it->second.size() : 0;
    }
    
    // 获取统计信息
    struct TokenStats {
        int total_tokens;
        int total_users;
        std::time_t oldest_token_time;
    };
    
    TokenStats get_stats() {
        std::shared_lock lock(store_mutex_);
        TokenStats stats;
        stats.total_tokens = token_store_.size();
        stats.total_users = user_tokens_index_.size();
        stats.oldest_token_time = std::numeric_limits<std::time_t>::max();
        
        for (const auto& [token, info] : token_store_) {
            if (info.created_at < stats.oldest_token_time) {
                stats.oldest_token_time = info.created_at;
            }
        }
        
        if (stats.oldest_token_time == std::numeric_limits<std::time_t>::max()) {
            stats.oldest_token_time = 0;
        }
        
        return stats;
    }
    
    const TokenConfig& get_config() const {
        return config_;
    }
};