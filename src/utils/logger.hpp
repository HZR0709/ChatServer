#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

enum LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& get_instance();
    
    void init(const std::string& filename, bool console_output = true);
    void set_log_level(LogLevel level);
    void log(LogLevel level, const std::string& message);
    
    // 便捷函数
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    
private:
    Logger();
    ~Logger();
    
    std::string get_timestamp();
    std::string level_to_string(LogLevel level);
    
    std::ofstream log_file_;
    std::mutex log_mutex_;
    LogLevel current_level_;
    bool console_output_;
    bool initialized_;
};

// 宏定义便于使用
#define LOG_DEBUG(msg) Logger::get_instance().debug(msg)
#define LOG_INFO(msg) Logger::get_instance().info(msg)
#define LOG_WARNING(msg) Logger::get_instance().warning(msg)
#define LOG_ERROR(msg) Logger::get_instance().error(msg)
#define LOG_CRITICAL(msg) Logger::get_instance().critical(msg)