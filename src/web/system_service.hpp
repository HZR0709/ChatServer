#pragma once
#include "monitor/server_monitor_impl.hpp"
#include "web/service_result.hpp"
#include "monitor/server_event_types.hpp"
#include "database/UserRepository.hpp"
#include <memory>
#include <chrono>

class SystemService {
private:
    std::shared_ptr<IServerMonitor> monitor_;
    std::shared_ptr<IUserRepository> user_repo_;

public:
    explicit SystemService(std::shared_ptr<IServerMonitor> monitor, 
                                std::shared_ptr<IUserRepository> user_repo)
        : monitor_(std::move(monitor)), user_repo_(std::move(user_repo)) {}

    ServiceResult<ServerStatistics> get_system_stats() {
        try {
            ServerStatistics stats;
            
            // 从监控器获取数据
            auto monitor_stats = monitor_->get_statistics();
            stats.cpu_usage = monitor_stats.cpu_usage;
            stats.memory_usage = monitor_stats.memory_usage;
            stats.memory_used_mb = 512;
            stats.memory_total_mb = 1024;
            stats.thread_count = monitor_stats.thread_count;
            stats.database_size = monitor_stats.database_size;
            stats.start_count_30d = 10;
            stats.server_status = "running";
            stats.This_uptime_seconds = monitor_->format_uptime();
           
            return ServiceResult<ServerStatistics>::success(stats, "获取系统统计成功");
        } catch (const std::exception& e) {
            return ServiceResult<ServerStatistics>::failure(std::string("获取系统统计失败: ") + e.what(), 500);
        }
    }

    ServiceResult<UserStats> get_user_stats() {
        UserStats stats;
        stats.total_users = user_repo_->get_total_users_count();
        stats.online_users = user_repo_->get_online_users_count();
        stats.active_today = 0;
        stats.active_week = 0;
        return ServiceResult<UserStats>::success(stats, "获取用户统计数据成功");
    }

    ServiceResult<std::vector<std::string>> get_logs(int lines = 100) {
        if (lines <= 0 || lines > 1000) {
            return ServiceResult<std::vector<std::string>>::failure("日志行数超出范围", 400);
        }

        try {
            // 这里应该从日志系统获取，暂时返回模拟数据
            std::vector<std::string> logs;
            logs.push_back("[INFO] 服务器启动成功");
            logs.push_back("[INFO] 用户 admin 登录");
            logs.push_back("[DEBUG] 新客户端连接");
            
            return ServiceResult<std::vector<std::string>>::success(logs, "获取日志成功");
        } catch (const std::exception& e) {
            return ServiceResult<std::vector<std::string>>::failure(std::string("获取日志失败: ") + e.what(), 500);
        }
    }

    ServiceResult<bool> perform_garbage_collection() {
        LOG_INFO("执行垃圾回收");
        // 实际实现应该清理临时文件、缓存等
        return ServiceResult<bool>::success(true, "垃圾回收完成");
    }
};