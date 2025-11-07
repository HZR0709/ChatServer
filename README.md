ChatServer
轻量级 C++ 聊天服务器与管理面板

概览

语言：C++17（主） + 静态 Web 前端（HTML/JS）
功能：TCP 长度前缀协议聊天、用户管理、消息持久化（SQLite）、内置管理 Web 界面
仓库：https://github.com/HZR0709/ChatServer
主要特性

长度前缀二进制消息协议（uint32_t 网络序）
用户注册/登录、私聊、广播、消息历史
持久化：SQLite（users/messages/user_sessions 等表）
简单内置 HTTP 静态文件与 API（管理面板）
多线程：线程池用于并发任务处理；心跳/超时回收
可通过 server.conf 配置端口、线程数、超时等
技术栈

C++17（std::thread、mutex、condition_variable）
BSD sockets（socket/bind/listen/accept/send/recv）
SQLite3（本地单文件 DB）
OpenSSL（CMake 中必需，可用于扩展 TLS）
JSON 解析（项目内置/头文件：nlohmann/picojson/ModernJsonParser）
构建：CMake（主），makefile、build.sh 备用
架构总览

main -> core/ChatServer（生命周期管理）
NetworkManager：socket 封装（accept/send/recv）
ConnectionManager：连接元信息（活跃时间、超时检测）
ThreadPool：后端任务执行
Repositories（UserRepository / MessageRepository / SessionRepository）：DB 持久化封装
Managers（UserManager / MessageHistory 等）：运行时内存结构与高层业务
WebServer：静态文件服务 + API 路由
消息协议

帧格式：4 字节长度前缀（uint32_t，htonl），随后 N 字节 payload（通常 JSON 文本）
send_message：先 send(len) 再 send(payload)
receive_message：先 recv(4) 再 recv(payload)
数据库（SQLite） 主要表：

users(id PK, username UNIQUE, password_hash, created_at, last_login, is_online)
messages(id PK, from_user_id, to_user_id NULLABLE, message_type, content TEXT, created_at)
user_sessions(socket_fd 等)
admin_users、server_status 等
配置（server.conf） 示例字段：

port = 8888
thread_pool_size = 8
connection_timeout = 300
heartbeat_interval = 60
max_message_history = 1000
log_file = chat_server.log
database_path = chat_server.db
web_port = 8080
enable_web_interface = true
构建与运行 依赖（Ubuntu/Debian 举例）：

build-essential, cmake, libsqlite3-dev, libssl-dev
使用 CMake：

bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
# 可执行位于 build/bin/chat_server
./build/bin/chat_server
使用 makefile：

bash
make
./chat_server
Web 管理界面

静态资源位于 web/，内置 WebServer 会将 ./web 作为根目录（默认 web_port = 8080）
管理页面通过 API（如 /api/users, /api/messages, /api/stats 等）与后端通信（前端已实现）
已知问题与安全注意

SQLite 参数绑定：当前实现中部分代码将字符串 "NULL" 作为参数传入 SQL，导致将 "NULL" 字面量写入 DB，不是真正的 SQL NULL。建议在绑定时使用 sqlite3_bind_null。
recv 短读处理：NetworkManager::receive_message 需要循环读取直到获得期望字节数，当前实现假定一次 recv 能拿齐所有数据，存在短读风险。
WebServer 每连接 spawn thread（detach）：并发高时会导致线程爆炸/资源耗尽。建议替换为线程池或事件驱动。
密码与认证：请使用安全哈希（bcrypt/argon2），并使用 JWT（仓库含 jwt-cpp 头）做管理员/API 认证。
SQLite 并发写瓶颈：在高写负载下应考虑写缓冲/队列或迁移到更适合并发的 DB。
改进建议（优先级）

修复 recv 短读与 SQLite NULL 绑定（已在 PR 草案中）
将 WebServer 改为线程池或非阻塞 epoll 实现
在关键路径减少锁粒度（分段锁或无锁结构）
支持 TLS（OpenSSL）
使用 bcrypt/argon2 存储密码，使用 JWT 做 API 验证
开发者文档（模块快速映射）

src/core/server.* — 启动/主循环/客户端消息处理（LOGIN/SEND/BROADCAST 等）
src/network/* — NetworkManager、ConnectionManager（socket 与连接元数据）
src/thread/* — ThreadPool
src/database/* — SQLiteDatabase、Repositories
src/managers/* — UserManager、MessageHistory
src/utils/* — Logger、ConfigManager（配置解析）
src/web/* — WebServer、ModernJsonParser、WebAPI（路由）
如何贡献

提交 issue 描述问题/改进点
新特性/较大变更请先开 issue 讨论设计
PR 包含：描述、变更文件、测试或复现步骤、若影响界面则更新文档
许可证

仓库当前未包含 LICENSE。若需要开放源代码，请添加 MIT/Apache-2.0 等合适许可证。
附录

最小化客户端示例与压力测试脚本，请参见本 README 后续章节（或直接复制 repo 根目录下的 scripts/，我可以生成并提交到仓库）。
