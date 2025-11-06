#pragma once
#include "IDatabase.hpp"
#include <sqlite3.h>
#include <memory>
#include "utils/logger.hpp"

class SQLiteDatabase : public IDatabase {
public:
    SQLiteDatabase();
    ~SQLiteDatabase() override;
    
    // IDatabase 接口实现
    bool initialize(const std::string& db_path) override;
    void shutdown() override;
    bool execute_sql(const std::string& sql) override;
    QueryResult execute_query(const std::string& sql) override;
    bool execute_parameterized_query(const std::string& sql, 
                const std::vector<std::string>& params = {}) override;
    virtual QueryResult execute_parameterized_query_with_result(
                const std::string& sql, 
                const std::vector<std::string>& params = {}) override;
    bool is_initialized() const override { return initialized_; }
    
private:
    bool create_tables();
    static int query_callback(void* data, int argc, char** argv, char** col_names);
    
    sqlite3* db_;
    std::string db_path_;
    bool initialized_;
};