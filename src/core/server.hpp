#pragma once

#include "config/CompositeConfigProvider.hpp"
#include "config/ServerConfig.hpp"
#include "config/config_manager.hpp"
#include "database/AdminRepository.hpp"
#include "database/DatabaseFactory.hpp"
#include "database/MessageRepository.hpp"
#include "database/SessionRepository.hpp"
#include "database/UserRepository.hpp"
#include "managers/message_history.hpp"
#include "managers/user_manager.hpp"
#include "monitor/server_monitor_impl.hpp"
#include "network/connection_manager.hpp"
#include "network/network_manager.hpp"
#include "thread/thread_pool.hpp"
#include "types/chat_types.hpp"
#include "utils/logger.hpp"
#include "web/token_manager.hpp"
#include "web/web_api.hpp"
#include "web/web_server.hpp"
#include <atomic>
#include <memory>
#include <thread>

class ChatServer {
public:
    // 使用依赖注入
    explicit ChatServer(std::shared_ptr<ConfigManager> config_manager);
    ~ChatServer();

    bool initialize(const std::string& config_file = "server.conf");
    void run();
    void shutdown();
    bool is_running() const { return running_; }

private:
    // 初始化方法
    bool load_config();
    bool load_configuration(const std::string& config_file); 
    bool initialize_database();
    bool initialize_managers();
    bool initialize_network();
    bool initialize_web_interface();
    bool initialize_monitor();

    // 线程函数
    void heartbeat_thread();
    void input_thread_func();
    void maintenance_thread();
    void main_loop();

    // 客户端处理
    void handle_client(int client_fd);
    void handle_client_message(int client_fd, const std::string& message);

    // 工具函数
    std::vector<std::string> split_string(const std::string& str, char delimiter);

    // 信号处理
    static void signal_handler(int signal);

    // 成员变量
    std::atomic<bool>              running_;
    ServerConfig                   config_;

    // 核心组件
    std::unique_ptr<NetworkManager>    network_;
    std::unique_ptr<UserManager>       user_manager_;
    std::unique_ptr<ThreadPool>        thread_pool_;
    std::unique_ptr<MessageHistory>    message_history_;
    std::unique_ptr<ConnectionManager> connection_manager_;
    std::unique_ptr<WebServer>         web_server_;
    std::shared_ptr<IServerMonitor>    monitor_;
    std::shared_ptr<WebAPI>            webapi_;

    //
    std::shared_ptr<AdminService>   admin_service_;
    std::shared_ptr<MessageService> message_service_;
    std::shared_ptr<SystemService>  system_service_;
    std::shared_ptr<UserService>    user_service_;
    std::shared_ptr<CommandFactory> command_factory_;

    // 数据库仓储
    std::shared_ptr<IDatabase>
                                        database_;
    std::shared_ptr<IUserRepository>    user_repository_;
    std::shared_ptr<IMessageRepository> message_repository_;
    std::shared_ptr<ISessionRepository> session_repository_;
    std::shared_ptr<IAdminRepository>   admin_repository_;

    // 配置和Token管理
    std::shared_ptr<ConfigManager> config_manager_;
    std::shared_ptr<TokenManager>  token_manager_;
    // 线程
    std::thread heartbeat_thread_;
    std::thread input_thread_;
    std::thread maintenance_thread_;
    std::thread main_thread_;

    // 单例组件（通过引用访问）
    Logger& logger_;

    // 防止拷贝
    ChatServer(const ChatServer&)            = delete;
    ChatServer& operator=(const ChatServer&) = delete;
};