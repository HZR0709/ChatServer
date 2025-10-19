#include "utils/logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

Logger::Logger() 
    : current_level_(INFO), console_output_(true), initialized_(false) {}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& filename, bool console_output) {
    {
        std::lock_guard lock(log_mutex_);
    
        if (initialized_) {
            return;
        }
        
        log_file_.open(filename, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "无法打开日志文件: " << filename << std::endl;
            return;
        }
        
        console_output_ = console_output;
        initialized_ = true;
    }
    
    info("日志系统初始化完成");
}

void Logger::set_log_level(LogLevel level) {
    std::lock_guard lock(log_mutex_);
    current_level_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_ || !initialized_) {
        return;
    }
    
    std::lock_guard lock(log_mutex_);
    
    std::string log_entry = get_timestamp() + " [" + level_to_string(level) + "] " + message;
    
    // 输出到文件
    if (log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    }
    
    // 输出到控制台
    if (console_output_) {
        std::cout << log_entry << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(INFO, message);
}

void Logger::warning(const std::string& message) {
    log(WARNING, message);
}

void Logger::error(const std::string& message) {
    log(ERROR, message);
}

void Logger::critical(const std::string& message) {
    log(CRITICAL, message);
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        case CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}