#pragma once
#include <string>

struct UserStats {
    int total_users;
    int online_users;
    int active_today;
    int active_week;
};

// 服务器状态事件类型
enum class ServerEventType {
    START,          // 服务器启动
    SHUTDOWN,       // 正常关闭
    RESTART,        // 重启
    CRASH,          // 异常崩溃
    MAINTENANCE,    // 维护
    HEALTH_CHECK    // 健康检查
};

// 服务器状态记录
struct ServerStatusRecord {
    int id;                          ///< 记录的唯一标识符，数据库自增主键
    ServerEventType event_type;      ///< 事件类型枚举值（START, SHUTDOWN, RESTART, CRASH等）
    std::string event_time;          ///< 事件发生的时间戳，格式为"YYYY-MM-DD HH:MM:SS"
    int uptime_seconds;              ///< 本次会话的运行时长（秒），对于START事件为0
    std::string details;             ///< 事件详情描述，提供额外的上下文信息
    bool is_abnormal;                ///< 是否为异常事件标志，true表示异常（如崩溃）
    
    ServerStatusRecord();
};

// 服务器统计信息
struct ServerStatistics {
    std::string This_uptime_seconds;         ///< 服务器本次启动行时间（秒）
    std::string total_uptime_seconds;        ///< 服务器历史总运行时间（秒），所有会话累加
    double availability_percentage;  ///< 服务器可用性百分比，基于异常事件比例计算
    int start_count_30d;             ///< 最近30天内的服务器启动次数
    int crash_count_30d;             ///< 最近30天内的服务器崩溃次数
    double average_uptime_30d;       ///< 最近30天内每次会话的平均运行时间（秒）
    ServerStatusRecord last_event;   ///< 最后一次记录的服务器事件
 
    // 添加系统资源监控
    double cpu_usage;           // CPU使用率百分比
    double memory_usage;        // 内存使用率百分比
    double memory_used_mb;      // 已用内存(MB)
    double memory_total_mb;     // 总内存(MB)
    int thread_count;           // 线程数
    std::string database_size;       // 数据库大小
    std::string server_status;
    std::vector<double> per_core_usage;  // 每个核心的使用率
    
    ServerStatistics() 
        : cpu_usage(0.0), memory_usage(0.0), 
          memory_used_mb(0.0), memory_total_mb(0.0) {}
};
