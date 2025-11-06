#pragma once
#include <string>
#include <regex>

class Validator {
public:
    static bool is_valid_username(const std::string& username) {
        if (username.length() < 3 || username.length() > 20) {
            return false;
        }
        
        // 只允许字母、数字、下划线
        std::regex pattern("^[a-zA-Z0-9_]+$");
        return std::regex_match(username, pattern);
    }

    static bool is_valid_email(const std::string& email) {
        if (email.empty()) {
            return true; // 邮箱可选
        }
        
        // 简单的邮箱验证
        std::regex pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
        return std::regex_match(email, pattern);
    }

    static bool is_valid_password(const std::string& password) {
        return password.length() >= 6;
    }

    static bool is_valid_user_id(const std::string& str) {
        try {
            int id = std::stoi(str);
            return id > 0;
        } catch (...) {
            return false;
        }
    }

    static bool is_valid_limit(const std::string& str, int max_limit = 1000) {
        try {
            int limit = std::stoi(str);
            return limit > 0 && limit <= max_limit;
        } catch (...) {
            return false;
        }
    }
};