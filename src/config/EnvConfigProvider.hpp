#pragma once
#include "BaseConfigProvider.hpp"
#include <cstdlib>
#include <iostream>

class EnvConfigProvider : public BaseConfigProvider {
public:
    bool load(const std::string& prefix = "") override {
        prefix_ = prefix;
        return true;
    }
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const override {
        std::string env_key = transform_key(key);
        const char* env_value = std::getenv(env_key.c_str());
        if (env_value) {
            return std::string(env_value);
        }
        return BaseConfigProvider::get_string(key, default_value);
    }
    
    int get_int(const std::string& key, int default_value = 0) const override {
        std::string env_key = transform_key(key);
        const char* env_value = std::getenv(env_key.c_str());
        if (env_value) {
            try {
                return std::stoi(env_value);
            } catch (const std::exception& e) {
                std::cerr << "环境变量 '" << env_key << "' 不是有效的整数: " << env_value << std::endl;
            }
        }
        return BaseConfigProvider::get_int(key, default_value);
    }
    
    bool get_bool(const std::string& key, bool default_value = false) const override {
        std::string env_key = transform_key(key);
        const char* env_value = std::getenv(env_key.c_str());
        if (env_value) {
            std::string value = to_lower(env_value);
            if (value == "true" || value == "1" || value == "yes" || value == "on") {
                return true;
            } else if (value == "false" || value == "0" || value == "no" || value == "off") {
                return false;
            } else {
                std::cerr << "环境变量 '" << env_key << "' 不是有效的布尔值: " << env_value << std::endl;
            }
        }
        return BaseConfigProvider::get_bool(key, default_value);
    }
    
    double get_double(const std::string& key, double default_value = 0.0) const override {
        std::string env_key = transform_key(key);
        const char* env_value = std::getenv(env_key.c_str());
        if (env_value) {
            try {
                return std::stod(env_value);
            } catch (const std::exception& e) {
                std::cerr << "环境变量 '" << env_key << "' 不是有效的浮点数: " << env_value << std::endl;
            }
        }
        return BaseConfigProvider::get_double(key, default_value);
    }
    
    bool contains(const std::string& key) const override {
        std::string env_key = transform_key(key);
        return std::getenv(env_key.c_str()) != nullptr || 
               BaseConfigProvider::contains(key);
    }

private:
    std::string prefix_;
    
    std::string transform_key(const std::string& key) const {
        std::string result = prefix_ + key;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        std::replace(result.begin(), result.end(), '.', '_');
        std::replace(result.begin(), result.end(), '-', '_');
        return result;
    }
};