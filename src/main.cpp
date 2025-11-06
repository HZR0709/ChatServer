#include <iostream>
#include <string>
#include "core/server.hpp"

int main() {
    try {
        DebugManager::register_signal_handler();
        auto config_manager = ConfigManager::create_from_ini("server.conf");
        if (!config_manager) {
            std::cerr << "配置加载失败" << std::endl;
            return 1;
        }
        ChatServer server(std::move(config_manager));
        if (!server.initialize()) {
            std::cerr << "服务器初始化失败" << std::endl;
            return 1;
        }
        server.run();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "服务器异常: " << e.what() << std::endl;
        return 1;
    }
}

