#pragma once
#include "IConfigProvider.hpp"
#include <memory>
#include <string>

class ConfigManager {
public:
    explicit ConfigManager(std::unique_ptr<IConfigProvider> provider);
    
    // 便捷构造方法
    static std::unique_ptr<ConfigManager> create_from_ini(const std::string& filename);
    static std::unique_ptr<ConfigManager> create_from_env(const std::string& prefix = "");
    static std::unique_ptr<ConfigManager> create_from_memory(
        const std::unordered_map<std::string, std::string>& initial_config = {});
    
    // 配置访问
    std::string get_string(const std::string& key, const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
    double get_double(const std::string& key, double default_value = 0.0) const;
    
    // 配置管理
    bool load(const std::string& source);
    bool reload();
    bool contains(const std::string& key) const;
    size_t size() const;
    
    // 验证
    bool validate_required(const std::vector<std::string>& required_keys) const;
    
    // 获取底层提供者
    IConfigProvider* get_provider() const { return provider_.get(); }

private:
    std::unique_ptr<IConfigProvider> provider_;
    std::string last_source_;
};