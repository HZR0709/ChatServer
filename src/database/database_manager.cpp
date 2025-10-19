#include "database/database_manager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include<functional>
#include "utils/logger.hpp"

DatabaseManager::DatabaseManager() 
    : db_(nullptr), initialized_(false) {}

DatabaseManager::~DatabaseManager() {
    shutdown();
}

DatabaseManager& DatabaseManager::get_instance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize(const std::string& db_path) {
    if (initialized_) {
        return true;
    }
    
    db_path_ = db_path;
    int rc = sqlite3_open(db_path.c_str(), &db_);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR("无法打开数据库: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    // 启用外键约束
    execute_sql("PRAGMA foreign_keys = ON;");
    
    // 创建表
    if (!create_tables()) {
        LOG_ERROR("创建数据库表失败");
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("数据库初始化成功: " + db_path);
    return true;
}

void DatabaseManager::shutdown() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    initialized_ = false;
}

bool DatabaseManager::create_tables() {
    const char* users_table_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username VARCHAR(50) UNIQUE NOT NULL,"
        "password_hash VARCHAR(128),"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_login DATETIME,"
        "is_online BOOLEAN DEFAULT 0"
        ");";
    
    const char* messages_table_sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "from_user_id INTEGER NOT NULL,"
        "to_user_id INTEGER,"
        "message_type INTEGER NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (from_user_id) REFERENCES users(id),"
        "FOREIGN KEY (to_user_id) REFERENCES users(id)"
        ");";
    
    const char* sessions_table_sql =
        "CREATE TABLE IF NOT EXISTS user_sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL,"
        "socket_fd INTEGER NOT NULL,"
        "login_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_activity DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "ip_address VARCHAR(45),"
        "FOREIGN KEY (user_id) REFERENCES users(id)"
        ");";
    
    const char* indexes_sql =
        "CREATE INDEX IF NOT EXISTS idx_messages_created_at ON messages(created_at);"
        "CREATE INDEX IF NOT EXISTS idx_messages_from_user ON messages(from_user_id);"
        "CREATE INDEX IF NOT EXISTS idx_messages_to_user ON messages(to_user_id);"
        "CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON user_sessions(user_id);";
    
    return execute_sql(users_table_sql) &&
           execute_sql(messages_table_sql) &&
           execute_sql(sessions_table_sql) &&
           execute_sql(indexes_sql);
}

bool DatabaseManager::execute_sql(const std::string& sql) {
    if (!db_) return false;
    
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL执行错误: " + std::string(error_msg));
        sqlite3_free(error_msg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::create_user(const std::string& username) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR IGNORE INTO users (username) VALUES (?);";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("准备SQL语句失败: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        LOG_ERROR("创建用户失败: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    LOG_DEBUG("用户创建成功: " + username);
    return true;
}

UserRecord DatabaseManager::get_user(const std::string& username) {
    UserRecord user;
    user.id = -1;
    
    if (!db_) return user;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users WHERE username = ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return user;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (sqlite3_column_text(stmt, 3)) {
            user.last_login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        }
        user.is_online = sqlite3_column_int(stmt, 4) != 0;
    }
    
    sqlite3_finalize(stmt);
    return user;
}

UserRecord DatabaseManager::get_user(int user_id) {
    UserRecord user;
    user.id = -1;
    
    if (!db_) return user;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users WHERE id = ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return user;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (sqlite3_column_text(stmt, 3)) {
            user.last_login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        }
        user.is_online = sqlite3_column_int(stmt, 4) != 0;
    }
    
    sqlite3_finalize(stmt);
    return user;
}

std::vector<UserRecord> DatabaseManager::get_all_users() {
    std::vector<UserRecord> users;
    
    if (!db_) return users;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, username, created_at, last_login, is_online FROM users ORDER BY username;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return users;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord user;
        user.id = sqlite3_column_int(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (sqlite3_column_text(stmt, 3)) {
            user.last_login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        }
        user.is_online = sqlite3_column_int(stmt, 4) != 0;
        users.push_back(user);
    }
    
    sqlite3_finalize(stmt);
    return users;
}

bool DatabaseManager::update_user_last_login(int user_id) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET last_login = datetime('now') WHERE id = ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::set_user_online_status(int user_id, bool online) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET is_online = ? WHERE id = ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, online ? 1 : 0);
    sqlite3_bind_int(stmt, 2, user_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::create_session(int user_id, int socket_fd, const std::string& ip_address) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO user_sessions (user_id, socket_fd, ip_address) VALUES (?, ?, ?);";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, socket_fd);
    sqlite3_bind_text(stmt, 3, ip_address.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // 更新用户在线状态
    if (rc == SQLITE_DONE) {
        set_user_online_status(user_id, true);
    }
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::update_session_activity(int socket_fd) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE user_sessions SET last_activity = datetime('now') WHERE socket_fd = ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, socket_fd);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::delete_session(int socket_fd) {
    if (!db_) return false;
    
    // 首先获取用户ID
    int user_id = -1;
    sqlite3_stmt* stmt;
    const char* select_sql = "SELECT user_id FROM user_sessions WHERE socket_fd = ?;";
    
    int rc = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, socket_fd);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // 删除会话
    const char* delete_sql = "DELETE FROM user_sessions WHERE socket_fd = ?;";
    rc = sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, socket_fd);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // 如果这是用户的最后一个会话，更新在线状态
    if (user_id != -1 && rc == SQLITE_DONE) {
        const char* count_sql = "SELECT COUNT(*) FROM user_sessions WHERE user_id = ?;";
        rc = sqlite3_prepare_v2(db_, count_sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int session_count = sqlite3_column_int(stmt, 0);
                if (session_count == 0) {
                    set_user_online_status(user_id, false);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::delete_all_sessions() {
    if (!db_) return false;
    
    // 将所有用户设置为离线
    execute_sql("UPDATE users SET is_online = 0;");
    
    // 删除所有会话
    return execute_sql("DELETE FROM user_sessions;");
}

bool DatabaseManager::save_message(int from_user_id, int to_user_id, int message_type, const std::string& content) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO messages (from_user_id, to_user_id, message_type, content) VALUES (?, ?, ?, ?);";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, from_user_id);
    if (to_user_id == -1) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_int(stmt, 2, to_user_id);
    }
    sqlite3_bind_int(stmt, 3, message_type);
    sqlite3_bind_text(stmt, 4, content.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<MessageRecord> DatabaseManager::get_recent_messages(int limit) {
    std::vector<MessageRecord> messages;
    
    if (!db_) return messages;
    
    sqlite3_stmt* stmt;
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "ORDER BY m.created_at DESC LIMIT ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return messages;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageRecord msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.from_user_id = sqlite3_column_int(stmt, 1);
        msg.to_user_id = sqlite3_column_int(stmt, 2);
        msg.message_type = sqlite3_column_int(stmt, 3);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.from_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (sqlite3_column_text(stmt, 7)) {
            msg.to_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        }
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    
    // 反转顺序，使最新的在最后
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<MessageRecord> DatabaseManager::get_user_messages(int user_id, int limit) {
    std::vector<MessageRecord> messages;
    
    if (!db_) return messages;
    
    sqlite3_stmt* stmt;
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "WHERE m.from_user_id = ? OR m.to_user_id = ? "
        "ORDER BY m.created_at DESC LIMIT ?;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return messages;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageRecord msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.from_user_id = sqlite3_column_int(stmt, 1);
        msg.to_user_id = sqlite3_column_int(stmt, 2);
        msg.message_type = sqlite3_column_int(stmt, 3);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.from_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (sqlite3_column_text(stmt, 7)) {
            msg.to_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        }
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    
    // 反转顺序，使最新的在最后
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<MessageRecord> DatabaseManager::get_messages_since(int message_id) {
    std::vector<MessageRecord> messages;
    
    if (!db_) return messages;
    
    sqlite3_stmt* stmt;
    const char* sql = 
        "SELECT m.id, m.from_user_id, m.to_user_id, m.message_type, m.content, m.created_at, "
        "u1.username as from_username, u2.username as to_username "
        "FROM messages m "
        "LEFT JOIN users u1 ON m.from_user_id = u1.id "
        "LEFT JOIN users u2 ON m.to_user_id = u2.id "
        "WHERE m.id > ? "
        "ORDER BY m.created_at ASC;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return messages;
    }
    
    sqlite3_bind_int(stmt, 1, message_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MessageRecord msg;
        msg.id = sqlite3_column_int(stmt, 0);
        msg.from_user_id = sqlite3_column_int(stmt, 1);
        msg.to_user_id = sqlite3_column_int(stmt, 2);
        msg.message_type = sqlite3_column_int(stmt, 3);
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.from_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (sqlite3_column_text(stmt, 7)) {
            msg.to_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        }
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return messages;
}

int DatabaseManager::get_message_count() {
    if (!db_) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM messages;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int DatabaseManager::get_total_users_count() {
    if (!db_) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM users;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int DatabaseManager::get_online_users_count() {
    if (!db_) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM users WHERE is_online = 1;";
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int DatabaseManager::get_total_messages_count() {
    return get_message_count();
}

std::string DatabaseManager::get_database_size() {
    if (!db_) return "0 KB";
    
    // 获取数据库文件大小
    std::ifstream file(db_path_, std::ifstream::ate | std::ifstream::binary);
    if (!file) {
        return "Unknown";
    }
    
    size_t size = file.tellg();
    file.close();
    
    // 转换为人类可读的格式
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_index = 0;
    double readable_size = size;
    
    while (readable_size >= 1024 && unit_index < 3) {
        readable_size /= 1024;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << readable_size << " " << units[unit_index];
    return ss.str();
}