#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

class IConfigProvider {
public:
    virtual ~IConfigProvider() = default;
    
    virtual bool load(const std::string& source) = 0;
    virtual std::string get_string(const std::string& key, const std::string& default_value = "") const = 0;
    virtual int get_int(const std::string& key, int default_value = 0) const = 0;
    virtual bool get_bool(const std::string& key, bool default_value = false) const = 0;
    virtual double get_double(const std::string& key, double default_value = 0.0) const = 0;
    
    virtual bool contains(const std::string& key) const = 0;
    virtual size_t size() const = 0;
    virtual void clear() = 0;
    
    virtual bool validate_required(const std::vector<std::string>& required_keys) const = 0;
    virtual std::unordered_map<std::string, std::string> get_all() const = 0;
};