#pragma once
#include "IConfigProvider.hpp"
#include "INIConfigProvider.hpp"
#include "EnvConfigProvider.hpp"
#include "MemoryConfigProvider.hpp"
#include <memory>

class ConfigFactory {
public:
    // 移除 create_provider 方法，直接使用具体的工厂方法
    
    static std::unique_ptr<IConfigProvider> create_ini_provider() {
        return std::make_unique<INIConfigProvider>();
    }
    
    static std::unique_ptr<IConfigProvider> create_env_provider(const std::string& prefix = "") {
        auto provider = std::make_unique<EnvConfigProvider>();
        provider->load(prefix);
        return provider;
    }
    
    static std::unique_ptr<IConfigProvider> create_memory_provider(
        const std::unordered_map<std::string, std::string>& initial_config = {}) {
        
        auto provider = std::make_unique<MemoryConfigProvider>();
        provider->load("");
        if (!initial_config.empty()) {
            // 我们需要将 MemoryConfigProvider 转换回具体类型来设置值
            if (auto memory_provider = dynamic_cast<MemoryConfigProvider*>(provider.get())) {
                memory_provider->set_values(initial_config);
            }
        }
        return provider;
    }
};