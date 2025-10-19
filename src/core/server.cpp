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

ChatServer::ChatServer() 
    : running_(false)
    , db_(DatabaseManager::get_instance())
    , logger_(Logger::get_instance())
    , config_manager_(ConfigManager::get_instance()) {
    
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
    
    // 加载配置
    if (!load_config(config_file)) {
        std::cerr << "配置加载失败" << std::endl;
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
        !initialize_web_interface()) {
        logger_.critical("服务器初始化失败");
        return false;
    }
    
    logger_.info("服务器初始化成功 - 端口: " + std::to_string(config_.port) + 
                 ", 线程数: " + std::to_string(config_.thread_pool_size) +
                 ", 数据库: " + config_.database_path);
    
    running_ = true;
    return true;
}

bool ChatServer::load_config(const std::string& config_file) {
    if (!config_manager_.load_config(config_file)) {
        std::cout << "使用默认配置..." << std::endl;
    }
    
    config_.port = config_manager_.get_int("port", 8888);
    config_.thread_pool_size = config_manager_.get_int("thread_pool_size", 4);
    config_.connection_timeout = config_manager_.get_int("connection_timeout", 300);
    config_.heartbeat_interval = config_manager_.get_int("heartbeat_interval", 60);
    config_.max_message_history = config_manager_.get_int("max_message_history", 1000);
    config_.log_file = config_manager_.get_string("log_file", "chat_server.log");
    config_.console_log = config_manager_.get_bool("console_log", true);
    config_.database_path = config_manager_.get_string("database_path", "chat_server.db");
    config_.web_port = config_manager_.get_int("web_port", 8080);
    config_.enable_web_interface = config_manager_.get_bool("enable_web_interface", true);
    
    return true;
}

bool ChatServer::initialize_database() {
    if (!db_.initialize(config_.database_path)) {
        logger_.critical("数据库初始化失败");
        return false;
    }
    
    // 清理之前的会话（服务器重启后）
    db_.delete_all_sessions();
    return true;
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
    
    web_server_ = std::make_unique<WebServer>();
    WebAPI::register_routes(*web_server_);
    
    if (!web_server_->start(config_.web_port)) {
        logger_.warning("Web服务器启动失败");
        return false;
    }
    
    logger_.info("Web管理界面已启动: http://localhost:" + std::to_string(config_.web_port));
    
    // 检查web目录是否存在
    std::ifstream test_file("./web/index.html");
    if (!test_file) {
        logger_.warning("Web目录不存在或index.html文件缺失，Web界面可能无法正常工作");
    } else {
        test_file.close();
    }
    
    return true;
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
            logger_.info("服务器状态 - 在线用户: " + std::to_string(user_manager_->get_user_count()) +
                        ", 总连接: " + std::to_string(connection_manager_->get_connection_count()) +
                        ", 数据库用户: " + std::to_string(db_.get_total_users_count()) +
                        ", 数据库消息: " + std::to_string(db_.get_total_messages_count()));
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
    db_.shutdown();
    
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
                db_.delete_session(fd);
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
            std::cout << "服务器状态:" << std::endl;
            std::cout << "  在线用户: " << user_manager_->get_user_count() << std::endl;
            std::cout << "  总连接数: " << connection_manager_->get_connection_count() << std::endl;
            std::cout << "  数据库用户: " << db_.get_total_users_count() << std::endl;
            std::cout << "  数据库消息: " << db_.get_total_messages_count() << std::endl;
            std::cout << "  数据库大小: " << db_.get_database_size() << std::endl;
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
                db_.delete_session(client_fd);
                
                // 广播用户下线消息
                auto online_users = user_manager_->get_online_users();
                std::string broadcast_msg = "系统: 用户 " + user->username + " 断开了连接";
                db_.save_message(-1, -1, 2, broadcast_msg);
                
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
        db_.update_session_activity(client_fd);
        
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
    
    // 这里简化处理，实际应该将命令处理也模块化
    if (command == "LOGIN") {
        if (tokens.size() < 2) {
            network_->send_message(client_fd, "错误: 缺少用户名");
            return;
        }
        
        const std::string& username = tokens[1];
        
        if (!db_.create_user(username)) {
            network_->send_message(client_fd, "错误: 用户创建失败");
            return;
        }
        
        auto user_record = db_.get_user(username);
        if (user_record.id == -1) {
            network_->send_message(client_fd, "错误: 用户不存在");
            return;
        }
        
        int user_id = user_manager_->add_user(client_fd, username);
        if (user_id < 0) {
            network_->send_message(client_fd, "错误: 用户已登录");
            return;
        }
        
        db_.create_session(user_id, client_fd);
        db_.update_user_last_login(user_id);
        
        network_->send_message(client_fd, "登录成功! 欢迎 " + username + " (ID: " + std::to_string(user_id) + ")");
        
        logger_.info("用户登录: " + username + " (ID: " + std::to_string(user_id) + ", FD: " + std::to_string(client_fd) + ")");
        
        // 广播用户上线消息
        auto online_users = user_manager_->get_online_users();
        std::string broadcast_msg = "系统: 用户 " + username + " 加入了聊天室";
        db_.save_message(-1, -1, 2, broadcast_msg);
        
        for (const auto& user : online_users) {
            if (user.user_id != user_id) {
                network_->send_message(user.socket_fd, broadcast_msg);
            }
        }
        
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
        int message_count = db_.get_total_messages_count();
        int thread_count = thread_pool_->get_thread_count();
        int task_count = thread_pool_->get_task_count();
        int total_users = db_.get_total_users_count();
        std::string db_size = db_.get_database_size();
        
        std::string stats = "=== 服务器统计 ===\n"
                           "在线用户: " + std::to_string(online_count) + "\n" +
                           "总连接数: " + std::to_string(connection_count) + "\n" +
                           "注册用户: " + std::to_string(total_users) + "\n" +
                           "总消息数: " + std::to_string(message_count) + "\n" +
                           "数据库大小: " + db_size + "\n" +
                           "工作线程: " + std::to_string(thread_count) + "\n" +
                           "待处理任务: " + std::to_string(task_count) + "\n" +
                           "=================";
        
        network_->send_message(client_fd, stats);
        
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