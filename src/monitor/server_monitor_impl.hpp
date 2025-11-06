#pragma once
#include "database/IDatabase.hpp"
#include "monitor/server_monitor_interface.hpp"
#include "monitor/CPUMonitor.hpp"
#include <memory>
#include <chrono>
#include <mutex>

class ServerMonitorImpl : public IServerMonitor {
public:
    explicit ServerMonitorImpl(std::shared_ptr<IDatabase> database);
    ~ServerMonitorImpl() override;
    
    // 禁止拷贝
    ServerMonitorImpl(const ServerMonitorImpl&) = delete;
    ServerMonitorImpl& operator=(const ServerMonitorImpl&) = delete;
    
    // IServerMonitor 接口实现
    bool initialize() override;
    void shutdown() override;
    
    void log_event(ServerEventType event_type, const std::string& details = "", bool is_abnormal = false) override;
    void log_start() override;
    void log_shutdown() override;
    void log_restart() override;
    void log_crash(const std::string& reason = "") override;
    void log_maintenance(const std::string& operation) override;
    void log_health_check() override;
    
    ServerStatistics get_statistics() override;
    std::vector<ServerStatusRecord> get_recent_events(int limit = 50) override;
    ServerStatusRecord get_last_event() override;
    
    void update_uptime() override;
    std::string format_uptime() const override;
    int get_current_uptime() const override;
    
    bool is_healthy() const override;
    std::string get_health_status() const override;

    // 系统资源监控方法
    double get_cpu_usage() override;
    double get_memory_usage() override;
    std::string get_database_size(const std::string& db_path_) override;
    std::vector<double> get_per_core_usage() override;
    void update_system_stats(ServerStatistics& stats) override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    std::shared_ptr<IDatabase> database_;
    // CPU监控
    std::unique_ptr<CPUMonitor> cpu_monitor_;
    mutable std::mutex stats_mutex_;
};