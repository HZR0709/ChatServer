#pragma once
#include "BaseConfigProvider.hpp"

class MemoryConfigProvider : public BaseConfigProvider {
public:
    bool load(const std::string& source) override {
        return true;
    }
    
    void set_values(const std::unordered_map<std::string, std::string>& values) {
        config_map_ = values;
    }
    
    void set_value(const std::string& key, const std::string& value) {
        config_map_[key] = value;
    }
    
    void update_values(const std::unordered_map<std::string, std::string>& values) {
        for (const auto& [key, value] : values) {
            config_map_[key] = value;
        }
    }
    
    bool remove_value(const std::string& key) {
        return config_map_.erase(key) > 0;
    }
};