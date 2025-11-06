#include "SQLiteDatabase.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

SQLiteDatabase::SQLiteDatabase() 
    : db_(nullptr), initialized_(false) {}

SQLiteDatabase::~SQLiteDatabase() {
    shutdown();
}

bool SQLiteDatabase::initialize(const std::string& db_path) {
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

void SQLiteDatabase::shutdown() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    initialized_ = false;
}

bool SQLiteDatabase::create_tables() {
    const char* users_table_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username VARCHAR(50) UNIQUE NOT NULL,"
        "password_hash VARCHAR(128),"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_login DATETIME,"
        "is_online BOOLEAN DEFAULT 0"
        ");";

    if (!execute_sql(users_table_sql)) {
        LOG_ERROR("创建【用户表】失败");
        return false;
    }
    
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
    
    if (!execute_sql(messages_table_sql)) {
        LOG_ERROR("创建【消息表】失败");
        return false;
    }

    const char* messages_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_messages_created_at ON messages(created_at);"
        "CREATE INDEX IF NOT EXISTS idx_messages_from_user ON messages(from_user_id);"
        "CREATE INDEX IF NOT EXISTS idx_messages_to_user ON messages(to_user_id);";

    if (!execute_sql(messages_index_sql)) {
        LOG_ERROR("创建【消息表索引】失败");
        return false;
    }

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
    
    if (!execute_sql(sessions_table_sql)) {
        LOG_ERROR("创建【用户会话表】失败");
        return false;
    }

    const char* sessions_index_sql = 
            "CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON user_sessions(user_id);";

    if (!execute_sql(sessions_index_sql)) {
        LOG_ERROR("创建【用户会话表索引】失败");
        return false;
    }

    const char* admin_users_table_sql =
        "CREATE TABLE IF NOT EXISTS admin_users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username VARCHAR(50) UNIQUE NOT NULL,"
        "password_hash VARCHAR(255) NOT NULL,"
        "email VARCHAR(100),"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_login DATETIME,"
        "is_active BOOLEAN DEFAULT 1"
        ");";
    
    if (!execute_sql(admin_users_table_sql)) {
        LOG_ERROR("创建【管理员用户表】失败");
        return false;
    } else {
        // create_default_admin();// 创建默认管理员账户（如果不存在）
    }
    

    const char* server_status_table_sql =
        "CREATE TABLE IF NOT EXISTS server_status ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "event_type VARCHAR(20) NOT NULL,"  // START, SHUTDOWN, RESTART, CRASH, MAINTENANCE
        "event_time DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "uptime_seconds INTEGER DEFAULT 0,"  // 本次运行时长（秒）
        "details TEXT,"                       // 事件详情
        "is_abnormal BOOLEAN DEFAULT 0"      // 是否为异常事件
        ");";
    
    if (!execute_sql(server_status_table_sql)) {
        LOG_ERROR("创建【服务器状态表】失败");
        return false;
    }
    
    const char* server_status_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_server_status_time ON server_status(event_time);"
        "CREATE INDEX IF NOT EXISTS idx_server_status_type ON server_status(event_type);";
    
    if (!execute_sql(server_status_index_sql)) {
        LOG_ERROR("创建【服务器状态表索引】失败");
        return false;
    }

    return true;
}

bool SQLiteDatabase::execute_sql(const std::string& sql) {
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

QueryResult SQLiteDatabase::execute_query(const std::string& sql) {
    QueryResult result;
    
    if (!db_) {
        result.success = false;
        return result;
    }
    
    char* error_msg = nullptr;
    
    struct CallbackData {
        std::vector<std::vector<std::string>>& rows;
        std::vector<std::string>& column_names;
        bool first_row = true;
    } data{result.rows, result.column_names};
    
    auto callback = [](void* user_data, int argc, char** argv, char** col_names) -> int {
        CallbackData* data = static_cast<CallbackData*>(user_data);
        
        if (data->first_row) {
            for (int i = 0; i < argc; i++) {
                data->column_names.push_back(col_names[i] ? col_names[i] : "");
            }
            data->first_row = false;
        }
        
        std::vector<std::string> row;
        for (int i = 0; i < argc; i++) {
            row.push_back(argv[i] ? argv[i] : "");
        }
        data->rows.push_back(row);
        
        return 0;
    };
    
    int rc = sqlite3_exec(db_, sql.c_str(), callback, &data, &error_msg);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR("查询执行失败: " + std::string(error_msg));
        sqlite3_free(error_msg);
        result.success = false;
    } else {
        result.success = true;
    }
    
    return result;
}

bool SQLiteDatabase::execute_parameterized_query(const std::string& sql, 
                                               const std::vector<std::string>& params) {
    if (!db_) return false;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("准备参数化查询失败: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    for (size_t i = 0; i < params.size(); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_STATIC);
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

QueryResult SQLiteDatabase::execute_parameterized_query_with_result(
    const std::string& sql, 
    const std::vector<std::string>& params) {
    
    QueryResult result;
    
    if (!db_) {
        result.success = false;
        return result;
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("准备参数化查询失败: " + std::string(sqlite3_errmsg(db_)));
        result.success = false;
        return result;
    }
    
    // 绑定参数
    for (size_t i = 0; i < params.size(); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_STATIC);
    }
    
    // 获取列名
    int column_count = sqlite3_column_count(stmt);
    for (int i = 0; i < column_count; i++) {
        result.column_names.push_back(sqlite3_column_name(stmt, i));
    }
    
    // 获取数据行
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < column_count; i++) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.push_back(text ? text : "");
        }
        result.rows.push_back(row);
    }
    
    if (rc != SQLITE_DONE) {
        LOG_ERROR("参数化查询执行失败: " + std::string(sqlite3_errmsg(db_)));
        result.success = false;
    } else {
        result.success = true;
    }
    
    sqlite3_finalize(stmt);
    return result;
}