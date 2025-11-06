#include "config/config_manager.hpp"
#include "INIConfigProvider.hpp"
#include "EnvConfigProvider.hpp"
#include "MemoryConfigProvider.hpp"

ConfigManager::ConfigManager(std::unique_ptr<IConfigProvider> provider)
    : provider_(std::move(provider)) {}

std::unique_ptr<ConfigManager> ConfigManager::create_from_ini(const std::string& filename) {
    auto provider = std::make_unique<INIConfigProvider>();
    auto manager = std::unique_ptr<ConfigManager>(new ConfigManager(std::move(provider)));
    if (!manager->load(filename)) {
        return nullptr;
    }
    return manager;
}

std::unique_ptr<ConfigManager> ConfigManager::create_from_env(const std::string& prefix) {
    auto provider = std::make_unique<EnvConfigProvider>();
    provider->load(prefix);
    return std::unique_ptr<ConfigManager>(new ConfigManager(std::move(provider)));
}

std::unique_ptr<ConfigManager> ConfigManager::create_from_memory(
    const std::unordered_map<std::string, std::string>& initial_config) {
    
    auto provider = std::make_unique<MemoryConfigProvider>();
    provider->load("");
    if (!initial_config.empty()) {
        if (auto memory_provider = dynamic_cast<MemoryConfigProvider*>(provider.get())) {
            memory_provider->set_values(initial_config);
        }
    }
    return std::unique_ptr<ConfigManager>(new ConfigManager(std::move(provider)));
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_value) const {
    return provider_->get_string(key, default_value);
}

int ConfigManager::get_int(const std::string& key, int default_value) const {
    return provider_->get_int(key, default_value);
}

bool ConfigManager::get_bool(const std::string& key, bool default_value) const {
    return provider_->get_bool(key, default_value);
}

double ConfigManager::get_double(const std::string& key, double default_value) const {
    return provider_->get_double(key, default_value);
}

bool ConfigManager::load(const std::string& source) {
    last_source_ = source;
    return provider_->load(source);
}

bool ConfigManager::reload() {
    if (last_source_.empty()) {
        return false;
    }
    return provider_->load(last_source_);
}

bool ConfigManager::contains(const std::string& key) const {
    return provider_->contains(key);
}

size_t ConfigManager::size() const {
    return provider_->size();
}

bool ConfigManager::validate_required(const std::vector<std::string>& required_keys) const {
    return provider_->validate_required(required_keys);
}