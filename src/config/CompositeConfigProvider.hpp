#pragma once
#include "IConfigProvider.hpp"
#include <vector>
#include <memory>

class CompositeConfigProvider : public IConfigProvider {
public:
    void add_provider(std::shared_ptr<IConfigProvider> provider) {
        providers_.push_back(provider);
    }
    
    bool load(const std::string& source) override {
        // 组合提供者不直接加载，而是由各个子提供者负责
        return true;
    }
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const override {
        for (const auto& provider : providers_) {
            if (provider->contains(key)) {
                return provider->get_string(key, default_value);
            }
        }
        return default_value;
    }
    
    int get_int(const std::string& key, int default_value = 0) const override {
        for (const auto& provider : providers_) {
            if (provider->contains(key)) {
                return provider->get_int(key, default_value);
            }
        }
        return default_value;
    }
    
    bool get_bool(const std::string& key, bool default_value = false) const override {
        for (const auto& provider : providers_) {
            if (provider->contains(key)) {
                return provider->get_bool(key, default_value);
            }
        }
        return default_value;
    }
    
    double get_double(const std::string& key, double default_value = 0.0) const override {
        for (const auto& provider : providers_) {
            if (provider->contains(key)) {
                return provider->get_double(key, default_value);
            }
        }
        return default_value;
    }
    
    bool contains(const std::string& key) const override {
        for (const auto& provider : providers_) {
            if (provider->contains(key)) {
                return true;
            }
        }
        return false;
    }
    
    size_t size() const override {
        size_t total = 0;
        for (const auto& provider : providers_) {
            total += provider->size();
        }
        return total;
    }
    
    void clear() override {
        for (auto& provider : providers_) {
            provider->clear();
        }
    }
    
    bool validate_required(const std::vector<std::string>& required_keys) const override {
        for (const auto& key : required_keys) {
            if (!contains(key)) {
                return false;
            }
        }
        return true;
    }
    
    std::unordered_map<std::string, std::string> get_all() const override {
        std::unordered_map<std::string, std::string> result;
        for (const auto& provider : providers_) {
            auto provider_config = provider->get_all();
            result.insert(provider_config.begin(), provider_config.end());
        }
        return result;
    }

private:
    std::vector<std::shared_ptr<IConfigProvider>> providers_;
};