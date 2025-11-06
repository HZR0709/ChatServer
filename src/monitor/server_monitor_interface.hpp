
#pragma once
#include "monitor/server_event_types.hpp" 
#include <vector>
#include <memory>

class IServerMonitor {
public:
    virtual ~IServerMonitor() = default;
    
    // 生命周期管理
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // 事件记录
    virtual void log_event(ServerEventType event_type, const std::string& details = "", bool is_abnormal = false) = 0;
    virtual void log_start() = 0;
    virtual void log_shutdown() = 0;
    virtual void log_restart() = 0;
    virtual void log_crash(const std::string& reason = "") = 0;
    virtual void log_maintenance(const std::string& operation) = 0;
    virtual void log_health_check() = 0;
    
    // 状态查询
    virtual ServerStatistics get_statistics() = 0;
    virtual std::vector<ServerStatusRecord> get_recent_events(int limit = 50) = 0;
    virtual ServerStatusRecord get_last_event() = 0;
    
    // 运行时间管理
    virtual void update_uptime() = 0;
    virtual std::string format_uptime() const  = 0;
    virtual int get_current_uptime() const = 0;
    
    // 健康状态
    virtual bool is_healthy() const = 0;
    virtual std::string get_health_status() const = 0;

    // 系统资源监控方法
    virtual double get_cpu_usage() = 0;
    virtual double get_memory_usage() = 0;
    virtual std::string get_database_size(const std::string& db_path_) = 0;
    virtual std::vector<double> get_per_core_usage() = 0;
    virtual void update_system_stats(ServerStatistics& stats) = 0;
};

// 创建智能指针别名
using IServerMonitorPtr = std::shared_ptr<IServerMonitor>;