#pragma once

#include <random>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <openssl/rand.h>

class SecretGenerator {
public:
    // 方法1：使用 OpenSSL 生成加密安全的随机密钥
    static std::string generate_secure_secret(int length = 32) {
        std::vector<unsigned char> buffer(length);
        
        if (RAND_bytes(buffer.data(), length) != 1) {
            throw std::runtime_error("无法生成加密安全的随机数");
        }
        
        std::stringstream ss;
        for (int i = 0; i < length; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(buffer[i]);
        }
        
        return ss.str();
    }
    
    // 方法2：使用 C++11 随机库生成密钥（适用于没有 OpenSSL 的环境）
    static std::string generate_random_secret(int length = 32) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "!@#$%^&*()_+-=[]{}|;:,.<>?";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
        
        std::string secret;
        secret.reserve(length);
        
        for (int i = 0; i < length; ++i) {
            secret += alphanum[dis(gen)];
        }
        
        return secret;
    }
    
    // 方法3：从环境变量或配置文件读取密钥
    static std::string get_secret_from_env(const std::string& env_var = "JWT_SECRET") {
        const char* secret = std::getenv(env_var.c_str());
        if (secret == nullptr || strlen(secret) == 0) {
            throw std::runtime_error("环境变量 " + env_var + " 未设置");
        }
        return std::string(secret);
    }
};