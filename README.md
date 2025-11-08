# ChatServer

轻量级 C++ 聊天服务器与管理面板（包含内置静态 Web 管理前端）

此 README 基于仓库源码自动生成，包含项目概览、模块分析、构建与运行说明、已知问题与修复建议、示例客户端与压力测试脚本说明等。

## 目录
- 概览
- 技术栈
- 总体架构与流程
- 关键模块详解
- 协议与消息格式
- 数据库模型（SQLite）
- 构建、运行与配置
- Web 管理界面
- 并发模型与实现细节
- 已知问题与安全注意
- 性能优化建议
- 测试与调试
- 部署与运维建议
- 扩展与二次开发建议
- 示例客户端与压力测试
- 贡献
- 许可证

## 概览
ChatServer 是一个用 C++（C++17）实现的轻量级聊天服务器，包含：
- 长连接聊天协议（长度前缀消息帧）
- 用户注册/登录、私聊、广播、消息历史
- SQLite 持久化（users/messages/user_sessions 等表）
- 简易内置 HTTP 静态文件服务与 API（用于管理面板）
- 线程池用于并发任务处理，心跳线程用于超时回收

项目结构（顶层）
- CMakeLists.txt / makefile / build.sh
- server.conf （运行配置）
- include/（第三方头，如 nlohmann, jwt-cpp）
- src/（核心 C++ 源码）
  - core/、network/、database/、managers/、thread/、web/、utils/ 等模块
- web/（静态管理前端：index.html 等）
- examples/（示例客户端，随后添加）
- tools/（压力测试脚本，随后添加）

## 技术栈
- 语言：C++17
- 网络：BSD sockets（socket, bind, listen, accept, send, recv）
- 并发：std::thread、std::mutex、std::condition_variable、自建 ThreadPool
- 数据库：SQLite3（sqlite3 C API）
- JSON：项目自带解析器 / 头（nlohmann/json / picojson / ModernJsonParser）
- 安全：项目包含 OpenSSL 配置依赖与 jwt-cpp 头（为进一步添加 TLS/JWT 做准备）
- 构建：CMake（主），也提供 makefile 与 build.sh

## 总体架构与流程
- main -> Core ChatServer（初始化配置/日志/数据库/网络/管理器/线程等）
- NetworkManager：创建监听 socket，接受连接，send/recv 协议封装（长度前缀）
- ConnectionManager：维护每个连接的元数据（last_activity、create_time），提供超时检测
- ThreadPool：工作线程执行业务任务（消息处理、DB 写入等）
- Repositories（UserRepository/MessageRepository/SessionRepository）：封装与 SQLite 的读写
- Managers（UserManager/MessageHistory）：内存层的运行态数据维护
- WebServer：静态文件服务 + 注册 API 路由（用于前端管理控制）

典型消息处理流程
1. accept 新连接 -> connection_manager.add_connection(fd)
2. 将 socket 交由线程池处理（线程池 worker 调用 handle_client）
3. receive_message（先读 4 字节长度，再读 body），解析命令（如 LOGIN|username 或 SEND|id|content）
4. 更新 session/activity，进行数据库写入或消息路由（私聊/广播）
5. 发送回复（send_message：先发 uint32_t 长度，再发 payload）

## 关键模块详解（精简）
- src/network/network_manager.*：length-prefixed 协议（4 字节网络序长度）用于消息边界，注意需处理短读/分片
- src/network/connection_manager.*：使用 mutex 保护 connections map，提供超时检索
- src/thread/thread_pool.*：基于条件变量的任务队列与 worker 线程
- src/database/SQLiteDatabase.*：封装 sqlite3_open/exec/prepare/step，负责表创建与查询封装
- src/database/MessageRepository.*、UserRepository.*：对 messages/users 表的读写封装
- src/managers/user_manager.*：运行时用户映射（fd -> user, user_id -> UserInfo）
- src/web/web_server.*：简易 HTTP 解析与静态文件服务，支持注册 GET/POST API handlers（每连接 spawn 线程）

## 协议与消息格式
- 帧格式：4 字节长度前缀（uint32_t，网络字节序）后接 N 字节 payload（建议 JSON 或项目现有简单文本命令）
- 服务器内置常用命令示例（以管道分隔）：
  - LOGIN|username
  - SEND|target_user_id|content
  - BROADCAST|content
  - LIST / HISTORY|n / STATS / QUIT / PING

## 数据库模型（SQLite）
主要表（在 src/database/SQLiteDatabase.cpp 的 create_tables 中定义）：
- users (id PK, username UNIQUE, password_hash, created_at, last_login, is_online)
- messages (id PK, from_user_id, to_user_id NULLABLE, message_type, content TEXT, created_at)
- user_sessions (id, user_id, socket_fd, login_time, last_activity, ip_address)
- admin_users (管理员信息)
- server_status (运行事件、uptime、details)

## 构建与运行
依赖（示例，Ubuntu/Debian）：
- build-essential, cmake, libsqlite3-dev, libssl-dev

使用 CMake（推荐）：
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
# 可执行文件位于 build/bin/chat_server
./build/bin/chat_server
```

使用 makefile：
```bash
make
./chat_server
```

配置（server.conf）：
- port = 8888
- thread_pool_size = 8
- connection_timeout = 300
- heartbeat_interval = 60
- max_message_history = 1000
- log_file = chat_server.log
- database_path = chat_server.db
- web_port = 8080
- enable_web_interface = true

## Web 管理界面
- 静态页面位于 web/，WebServer 将 ./web 作为 web 根（web_port 在 server.conf 中设置）
- 前端（web/index.html）提供管理员登录、用户管理、消息查看和服务器操作（重启/备份/清理等）

## 并发模型与实现细节
- ChatServer 使用主线程进行 accept + select（主循环），将客户端处理交给线程池
- WebServer 为每个 HTTP 连接启动独立线程（适用于低并发管理场景）
- 数据库（SQLite）在写密集场景可能成为瓶颈；考虑使用队列批量写或外部 DB

## 已知问题与安全注意（重要）
1. SQLite NULL 绑定问题：部分代码将字符串 "NULL" 作为参数绑定，导致在 DB 中保存字面 "NULL"（已在 fix 分支修复）
2. recv 短读/分片：receive_message 必须循环读取直至获得指定字节数（已在 fix 分支修复）
3. WebServer 每连接 spawn 线程：在高并发下会导致资源耗尽。建议改为线程池或事件驱动。
4. 密码与认证：未看到安全密码哈希（bcrypt/argon2），请勿在生产环境直接使用明文/弱哈希
5. TLS：建议在生产中启用 TLS（OpenSSL 已在 CMake 中列为依赖）

## 性能与安全优化建议（摘要）
- 将网络层改为非阻塞 + epoll/kqueue 的事件驱动模型以支持大并发
- 在写数据库时使用批量/异步写入或迁移到 Redis/MySQL 等更适合的存储
- 使用 bcrypt/argon2 存储密码，使用 JWT 做 API 认证（仓库已包含 jwt-cpp 头）
- 对关键路径减少锁竞争（分段锁、无锁队列），使用 writev 批量写

## 测试与调试
- 单元测试：建议引入 GoogleTest（gtest）做核心模块（ThreadPool/ConnectionManager/Repos 等）测试
- 压测工具：wrk、wrk2、ab 或提供的简单 Python stress_test 脚本（tools/stress_test.py）
- 性能分析：perf、Valgrind、AddressSanitizer、heaptrack

## 部署建议
- systemd 单元（示例）
```ini
[Unit]
Description=ChatServer
After=network.target

[Service]
Type=simple
User=chat
WorkingDirectory=/opt/chatserver
ExecStart=/opt/chatserver/bin/chat_server
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
- 容器化：建议基于 Debian/Ubuntu 镜像并安装 sqlite3 & OpenSSL dev 包构建
- 监控：导出连接数、消息入/出、延迟与 DB 写入时间，接入 Prometheus/Grafana

## 扩展建议
- 支持 WebSocket（便于浏览器端实时通信）
- 支持聊天房间/频道、消息撤回、离线消息推送
- 水平扩展：拆分消息处理与存储，使用 MQ/Redis 同步多实例

## 示例客户端与压力测试
- examples/python_client.py：最小 Python 客户端示例，展示如何发送带长度前缀的消息
- examples/node_client.js：Node.js 最小示例
- tools/stress_test.py：并发连接与消息发送的 Python 压力测试脚本（简单实现）

（示例脚本已添加到仓库 under examples/ and tools/）

## 贡献
- 欢迎提交 issue/PR。对于大改动，先开 issue 讨论设计
- 提交时包含：变更描述、测试说明、复现步骤（如有）
- 请在 PR 中保持清晰的 commit 历史（可选择不 squash）

## 许可证
- 当前仓库未包含 LICENSE 文件。若希望开源，请添加 MIT / Apache-2.0 / BSD 等许可证文件。
