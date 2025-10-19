cmake_minimum_required(VERSION 3.15)
project(ChatServer VERSION 1.0.0 LANGUAGES CXX)

# 设置C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS “${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")
else()
    set(CMAKE_CXX_FLAGS “${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
endif()

# 寻找依赖包
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 查找 SQLite3
find_path(SQLite3_INCLUDE_DIR sqlite3.h)
find_library(SQLite3_LIBRARY sqlite3)
if(SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)
    set(SQLite3_FOUND TRUE)
    set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
    set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
else()
    # 如果没找到系统安装的 SQLite3，可以使用内置的或报错
    message(FATAL_ERROR "SQLite3 not found. Please install sqlite3 development package.")
endif()

# 添加可执行文件
add_subdirectory(src)

# 安装目标
install(TARGETS chat_server
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(FILES server.conf
    DESTINATION etc/chatserver
)

# 安装 Web 文件
install(DIRECTORY web/
    DESTINATION share/chatserver/web
)

# 创建打包配置
set(CPACK_PACKAGE_NAME "ChatServer")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "A multi-threaded chat server with web interface")
set(CPACK_PACKAGE_CONTACT "developer@example.com")
set(CPACK_PACKAGE_VENDOR "ChatServer Team")
include(CPack)