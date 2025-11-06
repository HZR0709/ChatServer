# 包含头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 收集所有源文件
file(GLOB_RECURSE SRC_FILES
    "*.cpp"
)

# 按模块分组源文件（可选，便于调试）
set(CORE_SOURCES
    core/server.cpp
)

set(CONFIG_SOURCES
    config/config_manager.cpp
    config/ServerConfig.cpp
)

set(DATABASE_SOURCES
    # database/database_manager.cpp
    database/AdminRepository.cpp
    database/MessageRepository.cpp
    database/SessionRepository.cpp
    database/SQLiteDatabase.cpp
    database/UserRepository.cpp
)

set(MANAGER_SOURCES
    managers/user_manager.cpp
    managers/message_history.cpp
)

set(MONITOR_SOURCES
    monitor/server_monitor_impl.cpp
    monitor/CPUMonitor.cpp
)

set(NETWORK_SOURCES
    network/network_manager.cpp
    network/connection_manager.cpp
)

set(THREAD_SOURCES
    thread/thread_pool.cpp
)

set(UTILS_SOURCES
    # utils/config_manager.cpp
    utils/logger.cpp
)

set(WEB_SOURCES
    web/modern_json_parser.cpp
    web/web_server.cpp
    # web/web_api.cpp
)

# 创建可执行文件
add_executable(chat_server
    main.cpp
    ${CORE_SOURCES}
    ${CONFIG_SOURCES}
    ${DATABASE_SOURCES}
    ${MANAGER_SOURCES}
    ${NETWORK_SOURCES}
    ${THREAD_SOURCES}
    ${UTILS_SOURCES}
    ${WEB_SOURCES}
    ${MONITOR_SOURCES}
)

# 设置目标属性
target_include_directories(chat_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chat_server 
    PRIVATE 
        Threads::Threads
        ${SQLite3_LIBRARIES}
)

# 设置可执行文件的输出目录
set_target_properties(chat_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 添加编译定义（如果需要）
target_compile_definitions(chat_server PRIVATE 
    $<$<CONFIG:Debug>:DEBUG=1>
)

# 安装目标
install(TARGETS chat_server
    RUNTIME DESTINATION bin
)