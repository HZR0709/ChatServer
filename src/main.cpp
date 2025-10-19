#include <iostream>
#include <string>
#include "core/server.hpp"

int main(int argc, char* argv[]) {
    // 加载配置文件
    std::string config_file = "server.conf";
    
    // 简单的命令行参数解析
    if (argc > 1) {
        config_file = argv[1];
    }
    
    // 创建服务器实例
    ChatServer server;
    
    // 初始化服务器
    if (!server.initialize(config_file)) {
        std::cerr << "服务器初始化失败" << std::endl;
        return 1;
    }
    
    // 运行服务器
    server.run();
    
    // 服务器运行结束，正常退出
    return 0;
}

