#pragma once
#include "BaseConfigProvider.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

class INIConfigProvider : public BaseConfigProvider {
public:
    //  配置文件加载
    bool load(const std::string& filename) override {
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
    
    // 配置文件保存
    bool save(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        for (const auto& [key, value] : config_map_) {
            file << key << " = " << value << "\n";
        }
        
        file.close();
        return true;
    }
    
    //  配置值设置
    void set_value(const std::string& key, const std::string& value) {
        config_map_[key] = value;
    }

private:
    bool parse_line(const std::string& line, std::string& key, std::string& value) {
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
};