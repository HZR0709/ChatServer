#pragma once
#include "server_monitor_interface.hpp"
#include "server_monitor_impl.hpp"
#include <memory>

class ServerMonitorFactory {
public:
    // 创建默认实现
    static IServerMonitorPtr create() {
        return std::make_shared<ServerMonitorImpl>();
    }
    
    // 创建测试用的mock（仅在测试时使用）
    #ifdef UNIT_TEST
    template<typename MockType>
    static IServerMonitorPtr create_mock() {
        return std::make_shared<MockType>();
    }
    #endif
    
    // 创建空实现（用于禁用监控）
    static IServerMonitorPtr create_null() {
        class NullServerMonitor : public IServerMonitor {
        public:
            bool initialize() override { return true; }
            void shutdown() override {}
            void log_event(ServerEventType, const std::string&, bool) override {}
            void log_start() override {}
            void log_shutdown() override {}
            void log_restart() override {}
            void log_crash(const std::string&) override {}
            void log_maintenance(const std::string&) override {}
            void log_health_check() override {}
            ServerStatistics get_statistics() override { return ServerStatistics{}; }
            std::vector<ServerStatusRecord> get_recent_events(int) override { return {}; }
            ServerStatusRecord get_last_event() override { return ServerStatusRecord{}; }
            void update_uptime() override {}
            int get_current_uptime() const override { return 0; }
            bool is_healthy() const override { return true; }
            std::string get_health_status() const override { return "HEALTHY"; }
        };
        
        return std::make_shared<NullServerMonitor>();
    }
};