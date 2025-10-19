#include "web/web_api.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include "database/database_manager.hpp"
#include "utils/logger.hpp"

void WebAPI::register_routes(WebServer& server) {
    server.get("/api/stats", api_stats);
    server.get("/api/users", api_users);
    server.get("/api/messages", api_messages);
    server.get("/api/sessions", api_sessions);
    server.get("/api/system", api_system_info);
}

std::string WebAPI::api_stats(const std::map<std::string, std::string>& params) {
    auto& db = DatabaseManager::get_instance();
    
    int total_users = db.get_total_users_count();
    int online_users = db.get_online_users_count();
    int total_messages = db.get_total_messages_count();
    std::string db_size = db.get_database_size();
    
    std::stringstream json;
    json << "{"
         << "\"total_users\": " << total_users << ","
         << "\"online_users\": " << online_users << ","
         << "\"total_messages\": " << total_messages << ","
         << "\"database_size\": \"" << db_size << "\""
         << "}";
    
    return json_response(json.str());
}

std::string WebAPI::api_users(const std::map<std::string, std::string>& params) {
    auto& db = DatabaseManager::get_instance();
    auto users = db.get_all_users();
    
    std::stringstream json;
    json << "[";
    
    for (size_t i = 0; i < users.size(); ++i) {
        const auto& user = users[i];
        json << "{"
             << "\"id\": " << user.id << ","
             << "\"username\": \"" << user.username << "\","
             << "\"created_at\": \"" << user.created_at << "\","
             << "\"last_login\": \"" << (user.last_login.empty() ? "Never" : user.last_login) << "\","
             << "\"is_online\": " << (user.is_online ? "true" : "false")
             << "}";
        
        if (i < users.size() - 1) {
            json << ",";
        }
    }
    
    json << "]";
    
    return json_response(json.str());
}

std::string WebAPI::api_messages(const std::map<std::string, std::string>& params) {
    auto& db = DatabaseManager::get_instance();
    
    int limit = 50;
    auto it = params.find("limit");
    if (it != params.end()) {
        try {
            limit = std::stoi(it->second);
            if (limit > 1000) limit = 1000; // 防止过多数据
        } catch (...) {
            // 使用默认值
        }
    }
    
    auto messages = db.get_recent_messages(limit);
    
    std::stringstream json;
    json << "[";
    
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        json << "{"
             << "\"id\": " << msg.id << ","
             << "\"from_user_id\": " << msg.from_user_id << ","
             << "\"from_username\": \"" << msg.from_username << "\","
             << "\"to_user_id\": " << (msg.to_user_id == -1 ? "null" : std::to_string(msg.to_user_id)) << ","
             << "\"to_username\": \"" << (msg.to_username.empty() ? "所有人" : msg.to_username) << "\","
             << "\"message_type\": " << msg.message_type << ","
             << "\"content\": \"" << msg.content << "\","
             << "\"created_at\": \"" << msg.created_at << "\""
             << "}";
        
        if (i < messages.size() - 1) {
            json << ",";
        }
    }
    
    json << "]";
    
    return json_response(json.str());
}

std::string WebAPI::api_sessions(const std::map<std::string, std::string>& params) {
    // 注意：这里简化处理，实际应该从数据库查询会话信息
    // 由于我们已经在内存中维护了连接信息，这里返回空数组
    std::stringstream json;
    json << "[]";
    
    return json_response(json.str());
}

std::string WebAPI::api_system_info(const std::map<std::string, std::string>& params) {
    std::stringstream json;
    
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    // 获取服务器运行时间（这里简化处理）
    json << "{"
         << "\"server_time\": \"" << time_ss.str() << "\","
         << "\"version\": \"1.0.0\","
         << "\"status\": \"running\""
         << "}";
    
    return json_response(json.str());
}

std::string WebAPI::json_response(const std::string& data, bool success, const std::string& message) {
    std::stringstream json;
    json << "{"
         << "\"success\": " << (success ? "true" : "false") << ","
         << "\"message\": \"" << (message.empty() ? (success ? "OK" : "Error") : message) << "\","
         << "\"data\": " << data
         << "}";
    
    return json.str();
}

std::string WebAPI::error_response(const std::string& message) {
    return json_response("{}", false, message);
}