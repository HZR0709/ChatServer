#pragma once

#include <string>
#include <unordered_map>

class ConfigManager {
public:
    static ConfigManager& get_instance();
    
    bool load_config(const std::string& filename);
    std::string get_string(const std::string& key, const std::string& default_value = "");
    int get_int(const std::string& key, int default_value = 0);
    bool get_bool(const std::string& key, bool default_value = false);
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    std::unordered_map<std::string, std::string> config_map_;
    
    void trim(std::string& str);
    bool parse_line(const std::string& line, std::string& key, std::string& value);
};