#pragma once
#include <string>
#include <sstream>
#include <type_traits>
#include "nlohmann/json.hpp"
#include "web/service_result.hpp"
#include "database/UserRepository.hpp"
#include "monitor/server_event_types.hpp"
#include "database/MessageRepository.hpp"  // 包含你的数据模型
// #include "models/user.hpp"
// #include "admin_user.hpp"

class ApiResponse {
private:
    // 序列化辅助函数
    template<typename T>
    static nlohmann::json serialize_data(const T& data) {
        if constexpr (std::is_same_v<T, bool> ||
                     std::is_same_v<T, int> ||
                     std::is_same_v<T, double> ||
                     std::is_same_v<T, std::string>) {
            // 基础类型直接返回
            return data;
        } else if constexpr (std::is_same_v<T, std::vector<MessageRecord>>) {
            // 消息列表序列化
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& msg : data) {
                nlohmann::json msg_json;
                msg_json["id"] = msg.id;
                msg_json["from_user_id"] = msg.from_user_id;
                msg_json["from_username"] = msg.from_username;
                msg_json["to_user_id"] = msg.to_user_id == -1 ? nullptr : msg.to_user_id;
                msg_json["to_username"] = msg.to_username.empty() ? "所有人" : msg.to_username;
                msg_json["message_type"] = msg.message_type;
                msg_json["content"] = msg.content;
                msg_json["created_at"] = msg.created_at;
                msg_json["is_read"] = msg.is_read;
                json_array.push_back(msg_json);
            }
            return json_array;
        } else if constexpr (std::is_same_v<T, std::vector<UserRecord>>) {
            // 用户列表序列化
            nlohmann::json json_array = nlohmann::json::array();
            for (const auto& user : data) {
                nlohmann::json user_json;
                user_json["id"] = user.id;
                user_json["username"] = user.username;
                user_json["email"] = user.email;
                user_json["created_at"] = user.created_at;
                user_json["last_login"] = user.last_login.empty() ? "Never" : user.last_login;
                user_json["is_online"] = user.is_online;
                json_array.push_back(user_json);
            }
            return json_array;
        } else if constexpr (std::is_same_v<T, AdminUser>) {
            // 管理员用户序列化
            nlohmann::json user_json;
            user_json["id"] = data.id;
            user_json["username"] = data.username;
            user_json["email"] = data.email;
            user_json["token"] = data.token;
            user_json["created_at"] = data.created_at;
            user_json["last_login"] = data.last_login;
            return user_json;
        } else if constexpr (std::is_same_v<T, UserStats>) {
            // UserStats 序列化
            nlohmann::json stats_json;
            stats_json["total_users"] = data.total_users;
            stats_json["online_users"] = data.online_users;
            stats_json["active_today"] = data.active_today;
            stats_json["active_week"] = data.active_week;
            return stats_json;
        } else if constexpr (std::is_same_v<T, ServerStatistics>) {
            // SystemStats 序列化
            nlohmann::json stats_json;
            stats_json["cpu_usage"] = data.cpu_usage;
            stats_json["memory_usage"] = data.memory_usage;
            stats_json["active_connections"] = data.active_connections;
            stats_json["thread_count"] = data.thread_count;
            stats_json["queued_tasks"] = data.queued_tasks;
            stats_json["uptime"] = data.uptime;
            stats_json["server_status"] = data.server_status;
            stats_json["server_time"] = data.server_time;
            return stats_json;
        } else {// ... 添加更多特化
            // 默认情况：尝试使用 nlohmann::json 的自动序列化
            // 如果类型有 to_json 方法或者可以被自动转换，这会工作
            try {
                return data;
            } catch (const std::exception& e) {
                // 如果自动序列化失败，返回空对象并记录警告
                LOG_WARNING("无法序列化数据类型: " + std::string(typeid(T).name()));
                return nlohmann::json::object();
            }
        }
    }

public:
    static std::string success(const std::string& data = "{}", const std::string& message = "OK") {
        nlohmann::json response;
        response["success"] = true;
        response["message"] = message;
        try {
            response["data"] = nlohmann::json::parse(data);
        } catch (const std::exception& e) {
            // 如果解析失败，使用空对象
            response["data"] = nlohmann::json::object();
            LOG_WARNING("JSON解析失败: " + std::string(e.what()));
        }
        return response.dump();
    }

    static std::string success(const nlohmann::json& data, const std::string& message = "OK") {
        nlohmann::json response;
        response["success"] = true;
        response["message"] = message;
        response["data"] = data;
        return response.dump();
    }

    static std::string error(const std::string& message, int code = 0) {
        nlohmann::json response;
        response["success"] = false;
        response["message"] = message;
        response["error_code"] = code;
        response["data"] = nlohmann::json::object();
        return response.dump();
    }

    static std::string json(const nlohmann::json& data, bool success = true, const std::string& message = "") {
        nlohmann::json response;
        response["success"] = success;
        response["message"] = message.empty() ? (success ? "OK" : "Error") : message;
        response["data"] = data;
        return response.dump();
    }

    template<typename T>
    static std::string from_result(const ServiceResult<T>& result) {
        if (result.is_success()) {
            if constexpr (std::is_same_v<T, void>) {
                return success(nlohmann::json::object(), result.message());
            } else {
                if (result.data().has_value()) {
                    nlohmann::json data = serialize_data(result.data().value());
                    return success(data, result.message());
                } else {
                    // 理论上不应该发生，但为了安全
                    return success(nlohmann::json::object(), result.message());
                }
            }
        } else {
            return error(result.message(), result.error_code());
        }
    }

    // 专门处理基础类型的特化 - 保持向后兼容
    static std::string from_simple_result(const ServiceResult<bool>& result) {
        return from_result(result);
    }

    // 便捷方法：创建分页响应
    static std::string paginated(const nlohmann::json& data, 
                                int current_page, 
                                int page_size, 
                                int total_items,
                                const std::string& message = "OK") {
        nlohmann::json response;
        response["success"] = true;
        response["message"] = message;
        response["data"] = data;
        response["pagination"] = {
            {"current_page", current_page},
            {"page_size", page_size},
            {"total_items", total_items},
            {"total_pages", (total_items + page_size - 1) / page_size},
            {"has_next", current_page * page_size < total_items},
            {"has_prev", current_page > 1}
        };
        return response.dump();
    }

    // 便捷方法：创建列表响应
    static std::string list(const nlohmann::json& data, 
                           const std::string& message = "OK") {
        nlohmann::json response;
        response["success"] = true;
        response["message"] = message;
        response["data"] = {
            {"items", data},
            {"count", data.size()}
        };
        return response.dump();
    }

    // 便捷方法：创建单个对象响应
    static std::string single(const nlohmann::json& data, 
                             const std::string& message = "OK") {
        nlohmann::json response;
        response["success"] = true;
        response["message"] = message;
        response["data"] = data;
        return response.dump();
    }

    // 添加时间戳的响应
    static std::string with_timestamp(const nlohmann::json& data, 
                                     bool success = true, 
                                     const std::string& message = "") {
        nlohmann::json response;
        response["success"] = success;
        response["message"] = message.empty() ? (success ? "OK" : "Error") : message;
        response["data"] = data;
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return response.dump();
    }
};