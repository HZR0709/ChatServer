#pragma once

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <csignal>
#include <fstream>
#include <algorithm>
#include <sstream>

class DebugManager {
public:
    static DebugManager& get_instance() {
        static DebugManager instance;
        return instance;
    }

    // 启用/禁用调试类别
    void enable_category(const std::string& category, bool enabled = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        categories_[category] = enabled;
    }

    bool is_category_enabled(const std::string& category) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = categories_.find(category);
        return it != categories_.end() ? it->second : false;
    }

    // 设置全局调试级别
    void set_global_level(int level) { global_level_ = level; }
    int  get_global_level() const { return global_level_; }

    // 检查是否应该输出调试信息
    bool should_debug(const std::string& category, int level = 1) const {
        return level <= global_level_ && is_category_enabled(category);
    }

    // 从环境变量初始化
    void init_from_env() {
        const char* debug_categories = std::getenv("DEBUG_CATEGORIES");
        const char* debug_level      = std::getenv("DEBUG_LEVEL");

        if (debug_categories) {
            std::string        categories_str(debug_categories);
            std::istringstream ss(categories_str);
            std::string        category;

            // 重置所有类别为禁用
            for (auto& [cat, enabled] : categories_) {
                enabled = false;
            }

            // 启用环境变量中指定的类别
            while (std::getline(ss, category, ',')) {
                // 去除空格
                category.erase(0, category.find_first_not_of(" \t"));
                category.erase(category.find_last_not_of(" \t") + 1);

                if (!category.empty()) {
                    enable_category(category, true);
                    std::cout << "启用调试类别: " << category << std::endl;
                }
            }

            // 特殊值 "all" 启用所有类别
            if (categories_str == "all") {
                for (auto& [cat, enabled] : categories_) {
                    enabled = true;
                }
                std::cout << "启用所有调试类别" << std::endl;
            }
        }

        if (debug_level) {
            try {
                int level = std::stoi(debug_level);
                set_global_level(level);
                std::cout << "设置调试级别: " << level << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "无效的调试级别: " << debug_level << std::endl;
            }
        }
    }

    void init_from_config(const std::string& config_file = "debug.conf") {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            // 跳过注释和空行
            if (line.empty() || line[0] == '#') continue;

            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key   = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                // 去除空格
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                if (key == "DEBUG_LEVEL") {
                    try {
                        set_global_level(std::stoi(value));
                    } catch (...) { }
                } else if (key.find("DEBUG_") == 0) {
                    std::string category = key.substr(6);
                    std::transform(category.begin(), category.end(),
                        category.begin(), ::tolower);
                    enable_category(category, (value == "1" || value == "true"));
                }
            }
        }
    }

    // 注册信号处理器
    static void register_signal_handler() {
        std::signal(SIGUSR1, increase_debug_level);
        std::signal(SIGUSR2, decrease_debug_level);
    }

private:
    DebugManager()
        : global_level_(0) { // 默认关闭调试
        // 初始化默认类别（默认都禁用）
        categories_["http"]     = false;
        categories_["auth"]     = false;
        categories_["api"]      = false;
        categories_["database"] = false;

        // 从环境变量初始化
        init_from_env();
    }

    static void increase_debug_level(int signal) {
        auto& instance = get_instance();
        int current = instance.get_global_level();
        instance.set_global_level(current + 1);
        std::cout << "调试级别增加到: " << (current + 1) << std::endl;
    }
    
    static void decrease_debug_level(int signal) {
        auto& instance = get_instance();
        int current = instance.get_global_level();
        if (current > 0) {
            instance.set_global_level(current - 1);
            std::cout << "调试级别减少到: " << (current - 1) << std::endl;
        }
    }

    mutable std::mutex          mutex_;
    std::map<std::string, bool> categories_;
    int                         global_level_;
};

// 调试宏
#define DEBUG_CATEGORY(category, level, message)                                           \
    do {                                                                                   \
        if (DebugManager::get_instance().should_debug(category, level)) {                  \
            std::cout << "[" << category << "][" << level << "] " << message << std::endl; \
        }                                                                                  \
    } while (0)

#define DEBUG_HTTP(level, message) DEBUG_CATEGORY("http", level, message)
#define DEBUG_AUTH(level, message) DEBUG_CATEGORY("auth", level, message)
#define DEBUG_API(level, message)  DEBUG_CATEGORY("api", level, message)
#define DEBUG_DB(level, message)   DEBUG_CATEGORY("database", level, message)
