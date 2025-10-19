#include "utils/config_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

ConfigManager& ConfigManager::get_instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << filename << std::endl;
        return false;
    }
    
    config_map_.clear();
    std::string line;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        trim(line);
        
        // 跳过空行和注释
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        std::string key, value;
        if (parse_line(line, key, value)) {
            config_map_[key] = value;
        } else {
            std::cerr << "配置文件语法错误，第 " << line_num << " 行: " << line << std::endl;
        }
    }
    
    file.close();
    std::cout << "配置文件加载成功: " << filename 
              << " (" << config_map_.size() << " 个配置项)" << std::endl;
    return true;
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_value) {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        return it->second;
    }
    return default_value;
}

int ConfigManager::get_int(const std::string& key, int default_value) {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            std::cerr << "配置项 '" << key << "' 不是有效的整数: " << it->second << std::endl;
        }
    }
    return default_value;
}

bool ConfigManager::get_bool(const std::string& key, bool default_value) {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        } else if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        } else {
            std::cerr << "配置项 '" << key << "' 不是有效的布尔值: " << it->second << std::endl;
        }
    }
    return default_value;
}

void ConfigManager::trim(std::string& str) {
    // 去除左侧空格
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    
    // 去除右侧空格
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

bool ConfigManager::parse_line(const std::string& line, std::string& key, std::string& value) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }
    
    key = line.substr(0, pos);
    value = line.substr(pos + 1);
    
    trim(key);
    trim(value);
    
    // 去除值两端的引号
    if (!value.empty() && ((value.front() == '"' && value.back() == '"') || 
                          (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.length() - 2);
    }
    
    return !key.empty();
}