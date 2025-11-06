#pragma once
#include "IDatabase.hpp"
#include "SQLiteDatabase.hpp"
#include <memory>

enum class DatabaseType {
    SQLite,
    // 可以扩展其他数据库类型：MySQL, PostgreSQL等
};

class DatabaseFactory {
public:
    static std::shared_ptr<IDatabase> createDatabase(DatabaseType type) {
        switch(type) {
            case DatabaseType::SQLite:
                return std::make_shared<SQLiteDatabase>();
            default:
                throw std::invalid_argument("不支持的数据库类型");
        }
    }
    
    static std::shared_ptr<IDatabase> createSQLiteDatabase() {
        return std::make_shared<SQLiteDatabase>();
    }
};