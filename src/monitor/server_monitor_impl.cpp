#include "server_monitor_impl.hpp"
// #include "database/database_manager.hpp"
#include "utils/logger.hpp"
#include <sqlite3.h>
#include <sstream>
#include <fstream>
#include <iomanip>

// PImpl 实现（基本保持原有逻辑）
class ServerMonitorImpl::Impl {
public:
    bool initialized = false;
    std::chrono::steady_clock::time_point start_time;
    int current_session_uptime = 0;

    Impl(std::shared_ptr<IDatabase> database) : database_(std::move(database)) {}

    bool create_table() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS server_status ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "event_type VARCHAR(20) NOT NULL,"
            "event_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "uptime_seconds INTEGER DEFAULT 0,"
            "details TEXT,"
            "is_abnormal BOOLEAN DEFAULT 0"
            ");";
        
        const char* index_sql =
            "CREATE INDEX IF NOT EXISTS idx_server_status_time ON server_status(event_time);"
            "CREATE INDEX IF NOT EXISTS idx_server_status_type ON server_status(event_type);";

        return database_->execute_sql(sql) && database_->execute_sql(index_sql);
    }

    bool insert_event_record(const ServerStatusRecord& record) {
        // 原有实现保持不变
        if (!database_) return false;  // 使用注入的依赖
        // auto& db = DatabaseManager::get_instance();
        std::string event_type_str = event_type_to_string(record.event_type);
        std::vector<std::string> params = {
            event_type_str,
            record.details,
            std::to_string(record.uptime_seconds),
            std::to_string(record.is_abnormal ? 1 : 0)
        };
        
        std::string sql = "INSERT INTO server_status (event_type, details, uptime_seconds, is_abnormal) VALUES (?, ?, ?, ?);";
        return database_->execute_parameterized_query(sql, params);
    }
    
    std::vector<ServerStatusRecord> get_recent_events_impl(int limit) {
        // 原有实现保持不变
        std::vector<ServerStatusRecord> records;
        // auto& db = DatabaseManager::get_instance();
        
        std::string sql = 
            "SELECT id, event_type, event_time, uptime_seconds, details, is_abnormal "
            "FROM server_status "
            "ORDER BY event_time DESC LIMIT " + std::to_string(limit) + ";";
        
        auto result = database_->execute_query(sql);
        if (!result.success) {
            return records;
        }
        
        for (const auto& row : result.rows) {
            if (row.size() >= 6) {
                ServerStatusRecord record;
                record.id = std::stoi(row[0]);
                record.event_type = string_to_event_type(row[1]);
                record.event_time = row[2];
                record.uptime_seconds = std::stoi(row[3]);
                record.details = row[4];
                record.is_abnormal = (row[5] == "1");
                records.push_back(record);
            }
        }
        
        return records;
    }
    
    std::string event_type_to_string(ServerEventType type) {
        // 原有实现保持不变
        switch (type) {
            case ServerEventType::START: return "START";
            case ServerEventType::SHUTDOWN: return "SHUTDOWN";
            case ServerEventType::RESTART: return "RESTART";
            case ServerEventType::CRASH: return "CRASH";
            case ServerEventType::MAINTENANCE: return "MAINTENANCE";
            case ServerEventType::HEALTH_CHECK: return "HEALTH_CHECK";
            default: return "UNKNOWN";
        }
    }
    
    ServerEventType string_to_event_type(const std::string& str) {
        // 原有实现保持不变
        if (str == "START") return ServerEventType::START;
        if (str == "SHUTDOWN") return ServerEventType::SHUTDOWN;
        if (str == "RESTART") return ServerEventType::RESTART;
        if (str == "CRASH") return ServerEventType::CRASH;
        if (str == "MAINTENANCE") return ServerEventType::MAINTENANCE;
        if (str == "HEALTH_CHECK") return ServerEventType::HEALTH_CHECK;
        return ServerEventType::START;
    }

private:
    std::shared_ptr<IDatabase> database_;
};

// 构造函数接收依赖
ServerMonitorImpl::ServerMonitorImpl(std::shared_ptr<IDatabase> database) 
    : pimpl_(std::make_unique<Impl>(std::move(database))) {}

ServerMonitorImpl::~ServerMonitorImpl() {
    shutdown();
}

bool ServerMonitorImpl::initialize() {
    if (pimpl_->initialized) return true;
    pimpl_->start_time = std::chrono::steady_clock::now();
    if (!pimpl_->create_table())
    {
        LOG_ERROR("服务器监控模块初始化失败");
        return false;
    }
    pimpl_->initialized = true;
    LOG_INFO("服务器监控模块初始化成功");
    return true;
}

void ServerMonitorImpl::shutdown() {
    pimpl_->initialized = false;
}

// 其他方法实现与原来基本相同，只是移除了单例相关的代码
void ServerMonitorImpl::log_event(ServerEventType event_type, const std::string& details, bool is_abnormal) {
    if (!pimpl_->initialized) return;
    
    ServerStatusRecord record;
    record.event_type = event_type;
    record.details = details;
    record.is_abnormal = is_abnormal;
    
    if (event_type == ServerEventType::START) {
        record.uptime_seconds = 0;
    } else {
        record.uptime_seconds = get_current_uptime();
    }
    
    if (pimpl_->insert_event_record(record)) {
        std::string event_str = pimpl_->event_type_to_string(event_type);
        LOG_INFO("记录服务器事件: " + event_str + " - " + details);
    }
}

void ServerMonitorImpl::log_start() {
    log_event(ServerEventType::START, "服务器启动");
}

void ServerMonitorImpl::log_shutdown() {
    log_event(ServerEventType::SHUTDOWN, "服务器正常关闭");
}

void ServerMonitorImpl::log_restart() {
    log_event(ServerEventType::RESTART, "服务器重启");
}

void ServerMonitorImpl::log_crash(const std::string& reason) {
    std::string details = "服务器异常崩溃";
    if (!reason.empty()) {
        details += ": " + reason;
    }
    log_event(ServerEventType::CRASH, details, true);
}

void ServerMonitorImpl::log_maintenance(const std::string& operation) {
    log_event(ServerEventType::MAINTENANCE, operation);
}

void ServerMonitorImpl::log_health_check() {
    log_event(ServerEventType::HEALTH_CHECK, "服务器健康检查");
}

ServerStatistics ServerMonitorImpl::get_statistics() {
    ServerStatistics stats{};
    
    if (!pimpl_->initialized) {
        return stats;
    }
    
    // 获取基本统计信息
    auto recent_events = get_recent_events(1);
    if (!recent_events.empty()) {
        stats.last_event = recent_events[0];
    }
    
    stats.This_uptime_seconds = get_current_uptime();
    stats.total_uptime_seconds = get_current_uptime(); // 简化实现
    stats.thread_count = 8;
    stats.database_size = get_database_size("chat_server.db");
    // 更新系统资源统计
    update_system_stats(stats);
    
    return stats;
}

std::vector<ServerStatusRecord> ServerMonitorImpl::get_recent_events(int limit) {
    if (!pimpl_->initialized) {
        return {};
    }
    return pimpl_->get_recent_events_impl(limit);
}

ServerStatusRecord ServerMonitorImpl::get_last_event() {
    auto events = get_recent_events(1);
    if (events.empty()) {
        return ServerStatusRecord();
    }
    return events[0];
}

void ServerMonitorImpl::update_uptime() {
    // 如果需要定期更新运行时间，可以在这里实现
}

// 
std::string ServerMonitorImpl::format_uptime() const {
    int total_seconds = get_current_uptime();
    
    if (total_seconds < 0) {
        return "未知";
    }
    
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    std::stringstream ss;
    
    if (hours > 0) {
        ss << hours << "小时";
    }
    if (minutes > 0) {
        ss << minutes << "分钟";
    }
    if (hours == 0 && minutes == 0) {
        ss << seconds << "秒";
    }
    
    return ss.str();
}

int ServerMonitorImpl::get_current_uptime() const {
    if (!pimpl_->initialized) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - pimpl_->start_time);
    return static_cast<int>(duration.count());
}

bool ServerMonitorImpl::is_healthy() const {
    // 简单的健康检查逻辑
    return pimpl_->initialized && (get_current_uptime() > 0);
}

std::string ServerMonitorImpl::get_health_status() const {
    if (!is_healthy()) {
        return "UNHEALTHY";
    }
    
    int uptime = get_current_uptime();
    if (uptime < 60) {
        return "STARTING";
    } else if (uptime < 300) {
        return "WARMING_UP";
    } else {
        return "HEALTHY";
    }
}

ServerStatusRecord::ServerStatusRecord() 
    : id(-1), event_type(ServerEventType::START), uptime_seconds(0), is_abnormal(false) {}

// 简单的内存使用率获取
double ServerMonitorImpl::get_memory_usage() {
    std::ifstream file("/proc/meminfo");
    std::string line;
    double mem_total = 0.0;
    double mem_available = 0.0;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        double value;
        std::string unit;
        
        iss >> key >> value >> unit;
        
        if (key == "MemTotal:") {
            mem_total = value / 1024.0; // 转换为MB
        } else if (key == "MemAvailable:") {
            mem_available = value / 1024.0; // 转换为MB
        }
    }
    
    if (mem_total > 0) {
        double mem_used = mem_total - mem_available;
        return (mem_used / mem_total) * 100.0;
    }
    
    return 0.0;
}

double ServerMonitorImpl::get_cpu_usage() {
    // 使用上面 CPUMonitor 的实现
    static CPUMonitor cpu_monitor;
    return cpu_monitor.get_cpu_usage();
}

std::string ServerMonitorImpl::get_database_size(const std::string& db_path_) {
    if (db_path_.empty()) return "0 KB";
    
    // 获取数据库文件大小
    std::ifstream file(db_path_, std::ifstream::ate | std::ifstream::binary);
    if (!file) {
        return "Unknown";
    }
    
    size_t size = file.tellg();
    file.close();
    
    // 转换为人类可读的格式
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_index = 0;
    double readable_size = size;
    
    while (readable_size >= 1024 && unit_index < 3) {
        readable_size /= 1024;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << readable_size << " " << units[unit_index];
    return ss.str();
}

std::vector<double> ServerMonitorImpl::get_per_core_usage() {
    static CPUMonitor cpu_monitor;
    return cpu_monitor.get_per_core_usage();
}

void ServerMonitorImpl::update_system_stats(ServerStatistics& stats) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats.cpu_usage = get_cpu_usage();
    stats.memory_usage = get_memory_usage();
    stats.per_core_usage = get_per_core_usage();
    
    // 计算内存使用量（简化实现）
    std::ifstream file("/proc/meminfo");
    std::string line;
    double mem_total = 0.0;
    double mem_available = 0.0;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        double value;
        std::string unit;
        
        iss >> key >> value >> unit;
        
        if (key == "MemTotal:") {
            mem_total = value / 1024.0;
            stats.memory_total_mb = mem_total;
        } else if (key == "MemAvailable:") {
            mem_available = value / 1024.0;
        }
    }
    
    stats.memory_used_mb = mem_total - mem_available;
}
