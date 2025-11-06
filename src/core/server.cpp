#include "core/server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

// 全局服务器实例（用于信号处理）
static ChatServer* g_server_instance = nullptr;

ChatServer::ChatServer(std::shared_ptr<ConfigManager> config_manager) 
    : running_(false)
    , logger_(Logger::get_instance())
    , config_manager_(std::move(config_manager))  {
    
    // 设置全局实例用于信号处理
    g_server_instance = this;
}

ChatServer::~ChatServer() {
    shutdown();
    g_server_instance = nullptr;
}

bool ChatServer::initialize(const std::string& config_file) {
    if (running_) {
        return true;
    }
    
    if (!config_manager_) {
        config_manager_ = ConfigManager::create_from_ini(config_file);
        if (!config_manager_) {
            // 如果INI文件加载失败，尝试使用环境变量
            config_manager_ = ConfigManager::create_from_env("CHAT_SERVER_");
        }
        if (!config_manager_) {
            // 如果环境变量也失败，使用内存默认配置
            std::unordered_map<std::string, std::string> default_config = {
                {"port", "8888"},
                {"thread_pool_size", "4"},
                {"database_path", "chat_server.db"}
                // 其他默认配置...
            };
            config_manager_ = ConfigManager::create_from_memory(default_config);
        }
    }
    
    if (!config_manager_) {
        std::cerr << "所有配置源都加载失败" << std::endl;
        return false;
    }
    
    // 初始化日志系统
    logger_.init(config_.log_file, config_.console_log);
    logger_.set_log_level(INFO);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    logger_.info("=== 聊天服务器启动 ===");
    
    // 初始化各个组件
    if (!initialize_database() ||
        !initialize_managers() ||
        !initialize_network() ||
        !initialize_monitor() ||
        !initialize_web_interface()) {
        logger_.critical("服务器初始化失败");
        return false;
    }
    
    logger_.info("服务器初始化成功 - 端口: " + std::to_string(config_.port) + 
                 ", 线程数: " + std::to_string(config_.thread_pool_size) +
                 ", 数据库: " + config_.database_path);
    
    monitor_->log_start();
    running_ = true;
    return true;
}

bool ChatServer::load_config() {
    // 从配置管理器加载服务器配置
    config_ = ServerConfig::load_from(config_manager_);
    
    logger_.info("服务器配置加载: " + config_.to_string());
    return true;
}

bool ChatServer::load_configuration(const std::string& config_file) {
    ServerConfig default_config;
    
    // 第一优先级: 配置文件
    config_manager_ = ConfigManager::create_from_ini(config_file);
    if (config_manager_) {
        ServerConfig loaded_config = ServerConfig::load_from(config_manager_);
        if (loaded_config.validate()) {
            config_ = loaded_config;
            return true;
        }
        logger_.warning("配置文件验证失败: " + config_file);
    }
    
    // 第二优先级: 环境变量
    config_manager_ = ConfigManager::create_from_env("CHAT_SERVER_");
    if (config_manager_) {
        ServerConfig env_config = ServerConfig::load_from(config_manager_);
        if (env_config.validate()) {
            config_ = env_config;
            logger_.info("使用环境变量配置");
            return true;
        }
    }
    
    // 最终回退: 默认配置
    auto default_map = ServerConfig::to_config_map(default_config);
    config_manager_ = ConfigManager::create_from_memory(default_map);
    if (!config_manager_) {
        return false;
    }
    
    config_ = default_config;
    logger_.info("使用默认配置");
    
    return true;
}

bool ChatServer::initialize_database() {
    try {
        // 使用工厂模式创建数据库
        database_ = DatabaseFactory::createSQLiteDatabase();
        
        if (!database_->initialize(config_.database_path)) {
            logger_.critical("数据库初始化失败");
            return false;
        }
        
        // 使用依赖注入创建仓储
        user_repository_ = std::make_shared<UserRepository>(database_);
        message_repository_ = std::make_shared<MessageRepository>(database_);
        session_repository_ = std::make_shared<SessionRepository>(database_);
        admin_repository_ = std::make_shared<AdminRepository>(database_);
        
        // 清理之前的会话（服务器重启后）
        session_repository_->delete_all_sessions();
        
        logger_.info("数据库和仓储初始化成功");
        return true;
        
    } catch (const std::exception& e) {
        logger_.critical("数据库初始化异常: " + std::string(e.what()));
        return false;
    }
}

bool ChatServer::initialize_managers() {
    try {
        user_manager_ = std::make_unique<UserManager>();
        thread_pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);
        message_history_ = std::make_unique<MessageHistory>(config_.max_message_history);
        connection_manager_ = std::make_unique<ConnectionManager>();
        return true;
    } catch (const std::exception& e) {
        logger_.critical("管理器初始化失败: " + std::string(e.what()));
        return false;
    }
}

bool ChatServer::initialize_network() {
    network_ = std::make_unique<NetworkManager>();
    return network_->initialize(config_.port);
}

bool ChatServer::initialize_web_interface() {
    if (!config_.enable_web_interface) {
        return true;
    }
    // 加载安全配置
    SecurityConfig security_config = SecurityConfig::load_default();
    
    // 创建TokenManager
    TokenConfig token_config = TokenConfig::from_security_config(security_config);
    token_manager_ = std::make_shared<TokenManager>(token_config);

    admin_service_ = std::make_shared<AdminService>(admin_repository_, token_manager_);
    message_service_ = std::make_shared<MessageService>(message_repository_);
    system_service_ = std::make_shared<SystemService>(monitor_, user_repository_);
    user_service_ = std::make_shared<UserService>(user_repository_);
    command_factory_ = std::make_shared<CommandFactory>();
    
    web_server_ = std::make_unique<WebServer>();
    webapi_ = std::make_shared<WebAPI>(
        admin_service_, message_service_, system_service_, 
        user_service_, token_manager_, command_factory_
    );
    // 设置Web根目录
    web_server_->set_web_root("./web");
    
    // 使用依赖注入注册 WebAPI 路由
    WebAPI::register_routes(*web_server_, 
                            webapi_);
    
    if (!web_server_->start(config_.web_port)) {
        logger_.warning("Web服务器启动失败");
        return false;
    }
    
    logger_.info("Web管理界面已启动: http://localhost:" + std::to_string(config_.web_port));
    return true;
}

bool ChatServer::initialize_monitor() {
    monitor_ = std::make_shared<ServerMonitorImpl>(database_);
    return monitor_->initialize();
}

void ChatServer::run() {
    if (!running_) {
        logger_.error("服务器未初始化，无法运行");
        return;
    }
    
    // 启动各个线程
    heartbeat_thread_ = std::thread(&ChatServer::heartbeat_thread, this);
    input_thread_ = std::thread(&ChatServer::input_thread_func, this);
    
    logger_.info("服务器进入主循环，等待客户端连接...");
    main_loop();
}

void ChatServer::main_loop() {
    int server_socket = network_->get_server_socket();
    
    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_.error("select错误: " + std::string(strerror(errno)));
            break;
        }
        
        if (!running_) {
            break;
        }
        
        if (activity == 0) {
            continue;
        }
        
        if (FD_ISSET(server_socket, &read_fds)) {
            int client_fd = network_->accept_connection();
            if (client_fd > 0) {
                thread_pool_->enqueue([this, client_fd]() {
                    this->handle_client(client_fd);
                });

                logger_.debug("接受新客户端连接: FD " + std::to_string(client_fd));
                
                // 显示连接状态
                if (user_manager_->get_user_count() > 0) {
                    logger_.info("当前在线用户: " + std::to_string(user_manager_->get_user_count()) +
                                ", 待处理任务: " + std::to_string(thread_pool_->get_task_count()));
                }
            }
        }
        
        // 定期显示服务器状态
        static auto last_status_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_status_time);
        
        if (duration.count() >= 60) {
            auto total_users = user_repository_->get_total_users_count();
            auto online_users = user_repository_->get_online_users_count();
            auto total_messages = message_repository_->get_message_count();
            
            logger_.info("服务器状态 - 在线用户: " + std::to_string(user_manager_->get_user_count()) +
                        ", 总连接: " + std::to_string(connection_manager_->get_connection_count()) +
                        ", 数据库用户: " + std::to_string(total_users) +
                        ", 数据库消息: " + std::to_string(total_messages));
            last_status_time = now;
        }
    }
    
    logger_.info("主循环已退出");
}

void ChatServer::shutdown() {
    if (!running_) {
        return;
    }
    
    logger_.info("正在关闭服务器...");
    running_ = false;
    
    // 唤醒输入线程
    if (input_thread_.joinable()) {
        std::cout << std::endl;
        input_thread_.join();
    }
    
    // 关闭网络连接
    if (network_) {
        network_->shutdown();
    }
    
    // 等待心跳线程
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    
    // 关闭Web服务器
    if (web_server_) {
        web_server_->stop();
    }
    
    // 关闭线程池
    if (thread_pool_) {
        // 线程池析构函数会等待所有任务完成
    }
    
    // 关闭数据库
    if (database_) {
        database_->shutdown();
    }
    
    logger_.info("服务器已安全关闭");
}

void ChatServer::heartbeat_thread() {
    logger_.info("心跳检测线程启动");
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.heartbeat_interval));
        
        if (!running_) break;
        
        // 检查超时连接
        auto timed_out_connections = connection_manager_->get_timed_out_connections(config_.connection_timeout);
        
        for (int fd : timed_out_connections) {
            logger_.warning("连接超时，断开: FD " + std::to_string(fd));
            
            // 移除用户
            UserInfo* user = user_manager_->find_user_by_fd(fd);
            if (user) {
                logger_.info("因超时移除用户: " + user->username + " (ID: " + std::to_string(user->user_id) + ")");
                user_manager_->remove_user_by_fd(fd);
                session_repository_->delete_session(fd);
                
                // 更新用户在线状态
                user_repository_->set_user_online_status(user->user_id, false);
            }
            
            network_->close_connection(fd);
            connection_manager_->remove_connection(fd);
        }
        
        if (!timed_out_connections.empty()) {
            logger_.info("心跳检测完成，断开 " + std::to_string(timed_out_connections.size()) + " 个超时连接");
        }
    }
    
    logger_.info("心跳检测线程退出");
}

void ChatServer::input_thread_func() {
    logger_.info("控制台输入线程启动");
    std::cout << "控制台命令已激活，输入 'help' 查看可用命令" << std::endl;
    std::cout << "> " << std::flush;
    
    while (running_) {
        std::string input;
        std::getline(std::cin, input);
        
        if (!running_) break;
        
        if (input.empty()) {
            std::cout << "> " << std::flush;
            continue;
        }
        
        if (input == "quit" || input == "exit") {
            std::cout << "正在关闭服务器..." << std::endl;
            running_ = false;
            break;
        } else if (input == "status") {
            auto total_users = user_repository_->get_total_users_count();
            auto online_users = user_repository_->get_online_users_count();
            auto total_messages = message_repository_->get_message_count();
            
            std::cout << "服务器状态:" << std::endl;
            std::cout << "  在线用户: " << user_manager_->get_user_count() << std::endl;
            std::cout << "  总连接数: " << connection_manager_->get_connection_count() << std::endl;
            std::cout << "  数据库用户: " << total_users << std::endl;
            std::cout << "  数据库消息: " << total_messages << std::endl;
            std::cout << "  工作线程: " << thread_pool_->get_thread_count() << std::endl;
            std::cout << "  待处理任务: " << thread_pool_->get_task_count() << std::endl;
        } else if (input == "users") {
            auto users = user_manager_->get_online_users();
            std::cout << "在线用户列表 (" << users.size() << "):" << std::endl;
            for (const auto& user : users) {
                std::cout << "  ID:" << user.user_id << " - " << user.username 
                          << " (FD:" << user.socket_fd << ")" << std::endl;
            }
        } else if (input == "web") {
            if (web_server_ && web_server_->is_running()) {
                std::cout << "Web管理界面: http://localhost:" << config_.web_port << std::endl;
            } else {
                std::cout << "Web服务器未运行" << std::endl;
            }
        } else if (input == "help") {
            std::cout << "管理命令:" << std::endl;
            std::cout << "  status - 显示服务器状态" << std::endl;
            std::cout << "  users - 显示在线用户" << std::endl;
            std::cout << "  web - 显示Web管理界面URL" << std::endl;
            std::cout << "  quit - 退出服务器" << std::endl;
            std::cout << "  help - 显示此帮助" << std::endl;
        } else {
            std::cout << "未知命令，输入 'help' 查看可用命令" << std::endl;
        }
        
        if (running_) {
            std::cout << "> " << std::flush;
        }
    }
    
    logger_.info("控制台输入线程退出");
}

void ChatServer::maintenance_thread() {
    logger_.info("维护线程启动");
    // token_manager_ = std::make_unique<TokenManager>();
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::hours(1)); // 每小时执行一次
        
        if (!running_) break;
        
        try {
            // 清理过期令牌
            if (token_manager_) {
                token_manager_->cleanup_expired_tokens();
                size_t active_tokens = token_manager_->get_active_token_count();
                logger_.debug("当前活跃令牌数量: " + std::to_string(active_tokens));
            }
            
            // 其他维护任务...
            logger_.debug("维护任务执行完成");
        } catch (const std::exception& e) {
            logger_.error("维护任务执行失败: " + std::string(e.what()));
        }
    }
    
    logger_.info("维护线程退出");
}

void ChatServer::handle_client(int client_fd) {
    logger_.debug("开始处理客户端: FD " + std::to_string(client_fd));
    
    connection_manager_->add_connection(client_fd);
    
    // 发送欢迎消息
    network_->send_message(client_fd, "欢迎连接到多线程聊天服务器！请输入 LOGIN|用户名 登录");
    network_->send_message(client_fd, "可用命令: LOGIN, SEND, BROADCAST, LIST, HISTORY, STATS, QUIT, PING");
    
    while (running_) {
        std::string message = network_->receive_message(client_fd);
        
        if (message.empty()) {
            // 客户端断开连接
            UserInfo* user = user_manager_->find_user_by_fd(client_fd);
            if (user) {
                logger_.info("客户端断开连接: " + user->username + " (ID: " + std::to_string(user->user_id) + ")");
                user_manager_->remove_user_by_fd(client_fd);
                session_repository_->delete_session(client_fd);
                user_repository_->set_user_online_status(user->user_id, false);
                
                // 广播用户下线消息
                auto online_users = user_manager_->get_online_users();
                std::string broadcast_msg = "系统: 用户 " + user->username + " 断开了连接";
                message_repository_->save_message(-1, -1, 2, broadcast_msg);
                
                for (const auto& u : online_users) {
                    network_->send_message(u.socket_fd, broadcast_msg);
                }
            } else {
                logger_.debug("未登录客户端断开连接: FD " + std::to_string(client_fd));
            }
            
            network_->close_connection(client_fd);
            connection_manager_->remove_connection(client_fd);
            break;
        }
        
        logger_.debug("收到客户端消息 (FD " + std::to_string(client_fd) + "): " + message);
        
        // 更新活动时间
        connection_manager_->update_activity(client_fd);
        session_repository_->update_session_activity(client_fd);
        
        // 处理消息
        handle_client_message(client_fd, message);
        
        // 检查退出命令
        auto tokens = split_string(message, '|');
        if (!tokens.empty() && tokens[0] == "QUIT") {
            break;
        }
    }
    
    logger_.debug("客户端处理完成: FD " + std::to_string(client_fd));
}

void ChatServer::handle_client_message(int client_fd, const std::string& message) {
    auto tokens = split_string(message, '|');
    
    if (tokens.empty()) {
        network_->send_message(client_fd, "错误: 无效的消息格式");
        return;
    }
    
    const std::string& command = tokens[0];
    
    if (command == "LOGIN") {
        if (tokens.size() < 2) {
            network_->send_message(client_fd, "错误: 缺少用户名");
            return;
        }
        
        const std::string& username = tokens[1];
        
        if (!user_repository_->create_user(username)) {
            network_->send_message(client_fd, "错误: 用户创建失败");
            return;
        }
        
        auto user = user_repository_->get_user(username);
        if (!user.has_value()) {
            network_->send_message(client_fd, "错误: 用户不存在");
            return;
        }
        
        int user_id = user_manager_->add_user(client_fd, username);
        if (user_id < 0) {
            network_->send_message(client_fd, "错误: 用户已登录");
            return;
        }
        
        session_repository_->create_session(user_id, client_fd);
        user_repository_->update_user_last_login(user_id);
        user_repository_->set_user_online_status(user_id, true);
        
        network_->send_message(client_fd, "登录成功! 欢迎 " + username + " (ID: " + std::to_string(user_id) + ")");
        
        logger_.info("用户登录: " + username + " (ID: " + std::to_string(user_id) + ", FD: " + std::to_string(client_fd) + ")");
        
        // 广播用户上线消息
        auto online_users = user_manager_->get_online_users();
        std::string broadcast_msg = "系统: 用户 " + username + " 加入了聊天室";
        message_repository_->save_message(-1, -1, 2, broadcast_msg);
        
        for (const auto& user : online_users) {
            if (user.user_id != user_id) {
                network_->send_message(user.socket_fd, broadcast_msg);
            }
        }
        
    } else if (command == "SEND") {
        UserInfo* sender = user_manager_->find_user_by_fd(client_fd);
        if (!sender) {
            network_->send_message(client_fd, "错误: 请先登录");
            return;
        }
        
        if (tokens.size() < 3) {
            network_->send_message(client_fd, "错误: 缺少参数");
            return;
        }
        
        try {
            int target_user_id = std::stoi(tokens[1]);
            std::string content = tokens[2];
            
            UserInfo* target_user = user_manager_->find_user(target_user_id);
            if (!target_user) {
                network_->send_message(client_fd, "错误: 用户不存在或不在线");
                return;
            }
            
            std::string private_msg = "私聊来自 " + sender->username + " (ID:" + std::to_string(sender->user_id) + "): " + content;
            
            // 保存到数据库
            message_repository_->save_message(sender->user_id, target_user_id, 0, content);
            
            if (network_->send_message(target_user->socket_fd, private_msg)) {
                network_->send_message(client_fd, "私聊消息已发送给 " + target_user->username);
                logger_.debug("私聊消息: " + sender->username + " -> " + target_user->username + ": " + content);
            } else {
                network_->send_message(client_fd, "错误: 消息发送失败");
            }
            
        } catch (const std::exception& e) {
            network_->send_message(client_fd, "错误: 无效的用户ID");
        }
        
    } else if (command == "BROADCAST") {
        UserInfo* sender = user_manager_->find_user_by_fd(client_fd);
        if (!sender) {
            network_->send_message(client_fd, "错误: 请先登录");
            return;
        }
        
        if (tokens.size() < 2) {
            network_->send_message(client_fd, "错误: 缺少消息内容");
            return;
        }
        
        std::string content = tokens[1];
        std::string broadcast_msg = "广播来自 " + sender->username + " (ID:" + std::to_string(sender->user_id) + "): " + content;
        
        // 保存到数据库
        message_repository_->save_message(sender->user_id, -1, 1, content);
        
        // 广播给所有在线用户
        auto online_users = user_manager_->get_online_users();
        bool success = true;
        int send_count = 0;
        
        for (const auto& user : online_users) {
            if (network_->send_message(user.socket_fd, broadcast_msg)) {
                send_count++;
            } else {
                success = false;
            }
        }
        
        if (success) {
            network_->send_message(client_fd, "广播消息已发送给 " + std::to_string(send_count) + " 个用户");
        } else {
            network_->send_message(client_fd, "警告: 部分用户消息发送失败，成功发送 " + std::to_string(send_count) + " 个用户");
        }
        
        logger_.debug("广播消息: " + sender->username + ": " + content + " [接收者: " + std::to_string(send_count) + "]");
        
    } else if (command == "LIST") {
        auto online_users = user_manager_->get_online_users();
        
        if (online_users.empty()) {
            network_->send_message(client_fd, "当前没有在线用户");
            return;
        }
        
        std::string user_list = "=== 在线用户列表 (" + std::to_string(online_users.size()) + "人) ===\n";
        for (const auto& user : online_users) {
            user_list += "ID:" + std::to_string(user.user_id) + " - " + user.username + 
                        " (登录时间: " + user.login_time + ")\n";
        }
        user_list += "========================";
        
        network_->send_message(client_fd, user_list);
        
    } else if (command == "STATS") {
        int online_count = user_manager_->get_user_count();
        int connection_count = connection_manager_->get_connection_count();
        int message_count = message_repository_->get_message_count();
        int thread_count = thread_pool_->get_thread_count();
        int task_count = thread_pool_->get_task_count();
        int total_users = user_repository_->get_total_users_count();
        
        std::string stats = "=== 服务器统计 ===\n"
                           "在线用户: " + std::to_string(online_count) + "\n" +
                           "总连接数: " + std::to_string(connection_count) + "\n" +
                           "注册用户: " + std::to_string(total_users) + "\n" +
                           "总消息数: " + std::to_string(message_count) + "\n" +
                           "工作线程: " + std::to_string(thread_count) + "\n" +
                           "待处理任务: " + std::to_string(task_count) + "\n" +
                           "=================";
        
        network_->send_message(client_fd, stats);
        
    } else if (command == "HISTORY") {
        size_t count = 10;
        if (tokens.size() > 1) {
            try {
                count = std::stoi(tokens[1]);
                if (count > 50) count = 50; // 限制最大数量
            } catch (...) {
                // 使用默认值
            }
        }
        
        auto recent_messages = message_repository_->get_recent_messages(count);
        
        if (recent_messages.empty()) {
            network_->send_message(client_fd, "暂无消息历史");
            return;
        }
        
        network_->send_message(client_fd, "=== 最近 " + std::to_string(recent_messages.size()) + " 条消息 ===");
        for (const auto& msg : recent_messages) {
            std::string target_info = (msg.to_user_id == -1) ? "[广播]" : "[私聊]";
            std::string display_msg = "[" + msg.created_at + "] " + target_info + " " + msg.from_username + ": " + msg.content;
            network_->send_message(client_fd, display_msg);
        }
        network_->send_message(client_fd, "========================");
        
    } else if (command == "QUIT") {
        UserInfo* user = user_manager_->find_user_by_fd(client_fd);
        if (user) {
            std::string username = user->username;
            user_manager_->remove_user_by_fd(client_fd);
            
            // 从数据库删除会话和更新状态
            session_repository_->delete_session(client_fd);
            user_repository_->set_user_online_status(user->user_id, false);
            
            logger_.info("用户退出: " + username + " (ID: " + std::to_string(user->user_id) + ")");
            
            // 广播用户下线消息
            auto online_users = user_manager_->get_online_users();
            std::string broadcast_msg = "系统: 用户 " + username + " 离开了聊天室";
            message_repository_->save_message(-1, -1, 2, broadcast_msg);
            
            for (const auto& u : online_users) {
                network_->send_message(u.socket_fd, broadcast_msg);
            }
        }
        
        network_->send_message(client_fd, "已断开服务器...");
        network_->close_connection(client_fd);
        connection_manager_->remove_connection(client_fd);
        
    } else {
        // 其他命令处理...
        network_->send_message(client_fd, "命令处理: " + command);
    }
}

std::vector<std::string> ChatServer::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

void ChatServer::signal_handler(int signal) {
    if (g_server_instance) {
        g_server_instance->logger_.info("接收到信号 " + std::to_string(signal) + "，正在关闭服务器...");
        g_server_instance->running_ = false;
        std::cout << std::endl; // 唤醒输入线程
    }
}