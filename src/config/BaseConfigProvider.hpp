#pragma once
#include "IConfigProvider.hpp"
#include <algorithm>
#include <cctype>

class BaseConfigProvider : public IConfigProvider {
protected:
    std::unordered_map<std::string, std::string> config_map_;
    
    static void trim(std::string& str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
    }
    
    static std::string to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
public:
    std::string get_string(const std::string& key, const std::string& default_value = "") const override {
        auto it = config_map_.find(key);
        return it != config_map_.end() ? it->second : default_value;
    }
    
    int get_int(const std::string& key, int default_value = 0) const override {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            try {
                return std::stoi(it->second);
            } catch (const std::exception&) {
                // 记录错误或使用默认值
            }
        }
        return default_value;
    }
    
    bool get_bool(const std::string& key, bool default_value = false) const override {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            std::string value = to_lower(it->second);
            if (value == "true" || value == "1" || value == "yes" || value == "on") {
                return true;
            } else if (value == "false" || value == "0" || value == "no" || value == "off") {
                return false;
            }
        }
        return default_value;
    }
    
    double get_double(const std::string& key, double default_value = 0.0) const override {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            try {
                return std::stod(it->second);
            } catch (const std::exception&) {
                // 记录错误或使用默认值
            }
        }
        return default_value;
    }
    
    // 检查配置项是否存在
    bool contains(const std::string& key) const override {
        return config_map_.find(key) != config_map_.end();
    }
    
    // 获取配置项数量
    size_t size() const override {
        return config_map_.size();
    }
    
    // 清空所有配置
    void clear() override {
        config_map_.clear();
    }
    
    // 验证必需配置项是否存在
    bool validate_required(const std::vector<std::string>& required_keys) const override {
        for (const auto& key : required_keys) {
            if (!contains(key)) {
                return false;
            }
        }
        return true;
    }
    
    // 获取所有配置的副本
    std::unordered_map<std::string, std::string> get_all() const override {
        return config_map_;
    }
};