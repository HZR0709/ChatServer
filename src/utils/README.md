调试模块 (Debug Manager)
一个灵活、可配置的C++调试输出系统，支持多类别、多级别的运行时调试控制。

功能特性
🎯 多类别调试：支持HTTP、认证、API、数据库等不同模块的独立调试

📊 多级别控制：0-3级调试详细程度，从错误信息到完整数据转储

⚡ 运行时控制：无需重新编译即可启用/禁用调试

🔧 多种配置方式：环境变量、信号控制、文件触发

🛡️ 线程安全：使用互斥锁保证多线程环境安全

🔍 敏感信息保护：自动对token等敏感数据进行脱敏处理

快速开始
基础使用
cpp
#include "utils/debug_manager.hpp"

// 在代码中添加调试输出
DEBUG_HTTP(1, "收到HTTP请求");
DEBUG_AUTH(2, "用户认证过程");
DEBUG_API(3, "完整API响应数据");
编译要求
C++17 或更高版本

支持信号处理的POSIX系统（可选）

配置方式
1. 环境变量（推荐）
bash
# 启用HTTP和认证调试，级别1
export DEBUG_CATEGORIES=http,auth
export DEBUG_LEVEL=1

# 启用所有调试类别，级别2
export DEBUG_CATEGORIES=all
export DEBUG_LEVEL=2

# 关闭调试（默认）
unset DEBUG_CATEGORIES
unset DEBUG_LEVEL
2. 信号控制（动态调整）
bash
# 启动服务器后，动态调整调试级别
kill -SIGUSR1 <pid>    # 增加调试级别
kill -SIGUSR2 <pid>    # 减少调试级别
3. 配置文件
创建 debug.conf 文件：

ini
# 调试配置文件
DEBUG_LEVEL=2
DEBUG_HTTP=true
DEBUG_AUTH=true
DEBUG_API=false
DEBUG_DATABASE=false
调试级别说明
级别	描述	使用场景
0	错误信息	生产环境监控，只显示关键错误
1	主要步骤	日常开发，了解程序流程
2	详细信息	问题排查，查看处理细节
3	完整数据	深度调试，包含原始数据转储
调试类别
http - HTTP请求处理、路由、静态文件服务

auth - 用户认证、token验证、权限检查

api - API调用、业务逻辑处理

database - 数据库查询、连接管理

使用示例
基础调试输出
cpp
void handle_request(const std::string& request) {
    DEBUG_HTTP(1, "开始处理请求");
    
    // 解析请求
    DEBUG_HTTP(2, "请求方法: " + method);
    DEBUG_HTTP(2, "请求路径: " + path);
    
    // 认证检查
    DEBUG_AUTH(1, "开始用户认证");
    DEBUG_AUTH(2, "token格式检查");
    
    // 业务处理
    DEBUG_API(1, "执行API逻辑");
    DEBUG_API(2, "处理结果: " + result);
    
    DEBUG_HTTP(1, "请求处理完成");
}
生产环境配置
bash
# 生产环境 - 只监控错误
export DEBUG_LEVEL=0
export DEBUG_CATEGORIES=http,auth,api

# 启动服务器
./server
开发环境配置
bash
# 开发环境 - 详细调试
export DEBUG_LEVEL=2
export DEBUG_CATEGORIES=all

# 启动服务器
./server
排查特定问题
bash
# 只关注HTTP相关问题
export DEBUG_LEVEL=3
export DEBUG_CATEGORIES=http

# 启动服务器
./server
API参考
调试宏
cpp
// 基础调试宏
DEBUG_CATEGORY("category", level, message)

// 预定义类别宏
DEBUG_HTTP(level, message)      // HTTP相关调试
DEBUG_AUTH(level, message)      // 认证相关调试  
DEBUG_API(level, message)       // API相关调试
DEBUG_DB(level, message)        // 数据库相关调试
DebugManager 类
cpp
// 单例访问
auto& debug_mgr = DebugManager::get_instance();

// 启用/禁用调试类别
debug_mgr.enable_category("http", true);

// 设置调试级别
debug_mgr.set_global_level(2);

// 检查是否应该输出调试
bool should_debug = debug_mgr.should_debug("http", 1);
高级功能
自定义调试类别
cpp
// 在 DebugManager 构造函数中添加新类别
DebugManager() : global_level_(0) {
    categories_["http"] = false;
    categories_["auth"] = false;
    categories_["api"] = false;
    categories_["database"] = false;
    categories_["custom"] = false;  // 添加自定义类别
    init_from_env();
}

// 使用自定义类别
#define DEBUG_CUSTOM(level, message) DEBUG_CATEGORY("custom", level, message)
信号处理器注册
cpp
int main() {
    // 注册信号处理器（支持动态调整调试级别）
    DebugManager::register_signal_handler();
    
    // 启动服务器...
    WebServer server;
    server.start(8080);
    
    return 0;
}
性能考虑
调试级别0-1对性能影响极小，适合生产环境

调试级别2在频繁调用的代码路径中可能有轻微性能影响

调试级别3可能产生大量输出，建议仅在排查问题时使用

最佳实践
生产环境：使用级别0，只监控关键错误

测试环境：使用级别1，监控主要流程

开发环境：使用级别2，查看详细处理过程

问题排查：使用级别3，获取完整上下文信息

对敏感信息（token、密码等）使用脱敏输出：

cpp
DEBUG_AUTH(2, "token长度: " + std::to_string(token.length()));  // 推荐
DEBUG_AUTH(3, "完整token: " + token);  // 谨慎使用，可能泄露敏感信息
故障排除
调试输出不显示
检查环境变量是否正确设置：

bash
echo $DEBUG_CATEGORIES
echo $DEBUG_LEVEL
确认调试级别设置正确：

cpp
// 如果设置 DEBUG_LEVEL=1，只有级别<=1的消息会显示
DEBUG_HTTP(2, "这条消息在级别1时不会显示");
检查类别名称拼写：

bash
# 正确
export DEBUG_CATEGORIES=http

# 错误（大小写敏感）
export DEBUG_CATEGORIES=HTTP
信号控制不工作
确认PID正确：

bash
ps aux | grep your_server
确认信号处理器已注册：

cpp
DebugManager::register_signal_handler();
检查信号名称：

bash
# 正确
kill -SIGUSR1 <pid>

# 也可以使用数字
kill -10 <pid>    # SIGUSR1
kill -12 <pid>    # SIGUSR2
许可证
