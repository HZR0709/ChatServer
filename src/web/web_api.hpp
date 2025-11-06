#pragma once

#include "admin_service.hpp"
#include "api_response.hpp"
#include "command_factory.hpp"
#include "database/IAdminRepository.hpp"
#include "database/IMessageRepository.hpp"
#include "database/IUserRepository.hpp"
#include "message_service.hpp"
#include "monitor/server_monitor_impl.hpp"
#include "system_service.hpp"
#include "token_manager.hpp"
#include "user_service.hpp"
#include "web_server.hpp"
#include <iostream>
#include <map>
#include <string>

class WebAPI {
private:
    // 服务实例
    std::shared_ptr<AdminService>   admin_service_;
    std::shared_ptr<MessageService> message_service_;
    std::shared_ptr<SystemService>  system_service_;
    std::shared_ptr<UserService>    user_service_;

    std::shared_ptr<TokenManager> token_manager_;
    // 命令工厂
    std::shared_ptr<CommandFactory> command_factory_;

    // 辅助方法
    std::optional<int> authenticate_request(const std::map<std::string, std::string>& params) {
        auto token_it = params.find("token");

        if (token_it == params.end()) {
            // std::cout<< "token_it:null"<< std::endl;
            return std::nullopt;
        }

        auto result = admin_service_->verify_token(token_it->second);
        if (!result.is_success()) {
            return std::nullopt;
        }

        return result.data()->id;
    }

    template <typename Func>
    std::string handle_authenticated_call(const std::map<std::string, std::string>& params, Func operation) {

        auto user_id = authenticate_request(params);

        if (!user_id.has_value()) {
            return ApiResponse::error("未授权访问", 401);
        }

        try {
            return operation();
        } catch (const std::exception& e) {
            LOG_ERROR("API调用异常: " + std::string(e.what()));
            return ApiResponse::error("内部服务器错误", 500);
        }
    }

    template <typename Func>
    std::string handle_service_call(Func operation) {
        try {
            return operation();
        } catch (const std::exception& e) {
            LOG_ERROR("服务调用异常: " + std::string(e.what()));
            return ApiResponse::error("内部服务器错误", 500);
        }
    }

    // API 处理函数
    std::string handle_admin_login(const std::map<std::string, std::string>& params) {
        return handle_service_call([&]() {
            auto username_it = params.find("username");
            auto password_it = params.find("password");

            if (username_it == params.end() || password_it == params.end()) {
                return ApiResponse::error("缺少用户名或密码", 400);
            }

            auto result = admin_service_->login(username_it->second, password_it->second);
            if (result.is_success()) {
                nlohmann::json data;
                data["id"]       = result.data()->user.id;
                data["username"] = result.data()->user.username;
                data["email"]    = result.data()->user.email;
                data["token"]    = result.data()->token;
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string handle_server_command(const std::string& command_name, const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto command = command_factory_->create_command(command_name);
            if (!command) {
                return ApiResponse::error("未知的命令: " + command_name, 400);
            }

            auto result = command->execute();
            return ApiResponse::from_simple_result(result);
        });
    }

public:
    WebAPI(std::shared_ptr<AdminService> admin_service,
        std::shared_ptr<MessageService>  message_service,
        std::shared_ptr<SystemService>   system_service,
        std::shared_ptr<UserService>     user_service,
        std::shared_ptr<TokenManager>    token_manager,
        std::shared_ptr<CommandFactory>  command_factory)
        : admin_service_(std::move(admin_service))
        , message_service_(std::move(message_service))
        , system_service_(std::move(system_service))
        , user_service_(std::move(user_service))
        , token_manager_(std::move(token_manager))
        , command_factory_(std::move(command_factory)) { }

    // API 方法 - 现在作为实例方法
    std::string api_admin_login(const std::map<std::string, std::string>& params) {
        return handle_admin_login(params);
    }

    std::string api_admin_register(const std::map<std::string, std::string>& params) {
        return handle_service_call([&]() {
            auto username_it = params.find("username");
            auto password_it = params.find("password");
            auto email_it    = params.find("email");

            if (username_it == params.end() || password_it == params.end()) {
                return ApiResponse::error("缺少用户名或密码", 400);
            }

            const std::string& email  = email_it != params.end() ? email_it->second : "";
            auto               result = admin_service_->register_admin(username_it->second, password_it->second, email);
            return ApiResponse::from_simple_result(result);
        });
    }

    std::string api_admin_logout(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto token_it = params.find("token");
            if (token_it == params.end()) {
                return ApiResponse::error("缺少令牌", 400);
            }

            auto result = admin_service_->logout(token_it->second);
            return ApiResponse::from_simple_result(result);
        });
    }

    std::string api_server_restart(const std::map<std::string, std::string>& params) {
        return handle_server_command("restart", params);
    }

    std::string api_server_shutdown(const std::map<std::string, std::string>& params) {
        return handle_server_command("shutdown", params);
    }

    std::string api_server_reload_config(const std::map<std::string, std::string>& params) {
        return handle_server_command("reload-config", params);
    }

    std::string api_database_backup(const std::map<std::string, std::string>& params) {
        return handle_server_command("database-backup", params);
    }

    std::string api_send_broadcast(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto message_it = params.find("message");
            if (message_it == params.end()) {
                return ApiResponse::error("缺少消息内容", 400);
            }

            auto user_id = authenticate_request(params);
            auto result  = message_service_->send_broadcast(message_it->second, user_id.value_or(-1));
            return ApiResponse::from_simple_result(result);
        });
    }

    std::string api_clear_old_messages(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            int  days_old = 30;
            auto days_it  = params.find("days");
            if (days_it != params.end()) {
                try {
                    days_old = std::stoi(days_it->second);
                } catch (...) {
                    // 使用默认值
                }
            }

            auto result = message_service_->clear_old_messages(days_old);
            return ApiResponse::from_simple_result(result);
        });
    }

    std::string api_export_messages(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            int  limit    = 1000;
            auto limit_it = params.find("limit");
            if (limit_it != params.end()) {
                try {
                    limit = std::stoi(limit_it->second);
                } catch (...) {
                    // 使用默认值
                }
            }

            auto result = message_service_->export_messages(limit);
            if (result.is_success()) {
                nlohmann::json data = nlohmann::json::array();
                for (const auto& msg : result.get_data()) {
                    nlohmann::json msg_json;
                    msg_json["id"]            = msg.id;
                    msg_json["from_user_id"]  = msg.from_user_id;
                    msg_json["from_username"] = msg.from_username;
                    msg_json["to_user_id"]    = msg.to_user_id == -1 ? nullptr : std::to_string(msg.to_user_id);
                    msg_json["to_username"]   = msg.to_username.empty() ? "所有人" : msg.to_username;
                    msg_json["message_type"]  = msg.message_type;
                    msg_json["content"]       = msg.content;
                    msg_json["created_at"]    = msg.created_at;
                    data.push_back(msg_json);
                }
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string api_performance(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto result = system_service_->get_system_stats();
            if (result.is_success()) {
                nlohmann::json data;
                data["cpu_usage"]    = result.data()->cpu_usage;
                data["memory_usage"] = result.data()->memory_usage;
                // data["active_connections"] = result.data()->active_connections;
                data["thread_count"] = result.data()->thread_count;
                // data["queued_tasks"] = result.data()->queued_tasks;
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string api_logs(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            int  lines    = 100;
            auto lines_it = params.find("lines");
            if (lines_it != params.end()) {
                try {
                    lines = std::stoi(lines_it->second);
                } catch (...) {
                    // 使用默认值
                }
            }

            auto result = system_service_->get_logs(lines);
            if (result.is_success()) {
                nlohmann::json data = result.data();
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string api_stats(const std::map<std::string, std::string>& params) {
        return handle_service_call([&]() {
            auto user_stats    = system_service_->get_user_stats();
            auto message_stats = message_service_->get_total_messages_count();
            auto system_stats  = system_service_->get_system_stats();

            if (!user_stats.is_success() || !message_stats.is_success() || !system_stats.is_success()) {
                return ApiResponse::error("获取统计信息失败", 500);
            }

            nlohmann::json data;
            data["total_users"]    = user_stats.data()->total_users;
            data["online_users"]   = user_stats.data()->online_users;
            data["active_today"]   = user_stats.data()->active_today;
            data["active_week"]    = user_stats.data()->active_week;
            data["total_messages"] = message_stats.data();
            data["server_status"]  = "running";
            data["uptime"]         = system_stats.data()->This_uptime_seconds;
            data["thread_count"]   = system_stats.data()->thread_count;
            data["database_size"]  = system_stats.data()->database_size;
            return ApiResponse::json(data, true, "获取统计信息成功");
        });
    }

    std::string api_users(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto result = user_service_->get_all_users();
            if (result.is_success()) {
                nlohmann::json data = nlohmann::json::array();
                for (const auto& user : result.get_data()) {
                    nlohmann::json user_json;
                    user_json["id"]         = user.id;
                    user_json["username"]   = user.username;
                    user_json["created_at"] = user.created_at;
                    user_json["last_login"] = user.last_login.empty() ? "Never" : user.last_login;
                    user_json["is_online"]  = user.is_online;
                    data.push_back(user_json);
                }
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string api_messages(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            int  limit    = 50;
            auto limit_it = params.find("limit");
            if (limit_it != params.end()) {
                try {
                    limit = std::stoi(limit_it->second);
                } catch (...) {
                    // 使用默认值
                }
            }

            auto result = message_service_->get_recent_messages(limit);
            if (result.is_success()) {
                nlohmann::json data = nlohmann::json::array();
                for (const auto& msg : result.get_data()) {
                    nlohmann::json msg_json;
                    msg_json["id"]            = msg.id;
                    msg_json["from_user_id"]  = msg.from_user_id;
                    msg_json["from_username"] = msg.from_username;
                    msg_json["to_user_id"]    = msg.to_user_id == -1 ? "nullptr" : std::to_string(msg.to_user_id);
                    msg_json["to_username"]   = msg.to_username.empty() ? "所有人" : msg.to_username;
                    msg_json["message_type"]  = msg.message_type;
                    msg_json["content"]       = msg.content;
                    msg_json["created_at"]    = msg.created_at;
                    data.push_back(msg_json);
                }
                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::error(result.message(), result.error_code());
            }
        });
    }

    std::string api_sessions(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            // 简化实现
            nlohmann::json data = nlohmann::json::array();
            return ApiResponse::json(data, true, "获取会话列表成功");
        });
    }

    std::string api_system_info(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto result = system_service_->get_system_stats();
            if (result.is_success()) {
                auto stats = result.unwrap();
                nlohmann::json data;

                data["version"]    = "1.0.0";
                data["start_time"] = "2025-10-18 16:00:00"; // 应该从监控器获取
                // 运行时间统计
                data["current_uptime_seconds"]  = stats.This_uptime_seconds;
                data["total_uptime_seconds"]    = stats.total_uptime_seconds;
                data["availability_percentage"] = stats.availability_percentage;
                data["start_count_30d"]         = stats.start_count_30d;
                data["crash_count_30d"]         = stats.crash_count_30d;
                data["average_uptime_30d"]      = stats.average_uptime_30d;

                // 最后事件信息 - 根据 ServerStatusRecord 结构体字段
                if (stats.last_event.id > 0) { // 通过 id 判断是否有有效事件
                    nlohmann::json last_event;
                    last_event["id"]             = stats.last_event.id;
                    last_event["event_type"]     = stats.last_event.event_type; // 可能需要转换为字符串
                    last_event["event_time"]     = stats.last_event.event_time;
                    last_event["uptime_seconds"] = stats.last_event.uptime_seconds;
                    last_event["details"]        = stats.last_event.details;
                    last_event["is_abnormal"]    = stats.last_event.is_abnormal;
                    data["last_event"]           = last_event;
                } else {
                    data["last_event"] = nullptr; // 或空对象 {}
                }

                // 系统资源监控
                data["status"]          = stats.server_status;
                data["cpu_usage"]       = stats.cpu_usage;
                data["memory_usage"]    = stats.memory_usage;
                data["memory_used_mb"]  = stats.memory_used_mb;
                data["memory_total_mb"] = stats.memory_total_mb;
                data["thread_count"]    = stats.thread_count;

                // 每个核心的使用率
                if (!stats.per_core_usage.empty()) {
                    data["per_core_usage"] = stats.per_core_usage;
                }

                return ApiResponse::json(data, true, result.message());
            } else {
                return ApiResponse::json({}, false, result.message());
            }
        });
    }

    std::string api_kick_all_users(const std::map<std::string, std::string>& params) {
        return handle_authenticated_call(params, [&]() {
            auto result = admin_service_->kick_all_users();
            return ApiResponse::from_simple_result(result);
        });
    }

    // 静态注册方法
    static void register_routes(WebServer& server, std::shared_ptr<WebAPI> api_instance) {
        // 使用lambda包装实例方法
        server.get("/api/stats", [api_instance](const auto& params) {
            return api_instance->api_stats(params);
        });

        server.get("/api/users", [api_instance](const auto& params) {
            return api_instance->api_users(params);
        });

        server.get("/api/messages", [api_instance](const auto& params) {
            return api_instance->api_messages(params);
        });

        server.get("/api/sessions", [api_instance](const auto& params) {
            return api_instance->api_sessions(params);
        });

        server.get("/api/system", [api_instance](const auto& params) {
            return api_instance->api_system_info(params);
        });

        // 认证路由
        server.post("/api/admin/login", [api_instance](const auto& params) {
            return api_instance->api_admin_login(params);
        });

        server.post("/api/admin/register", [api_instance](const auto& params) {
            return api_instance->api_admin_register(params);
        });

        server.post("/api/admin/logout", [api_instance](const auto& params) {
            return api_instance->api_admin_logout(params);
        });

        // 服务器控制
        server.post("/api/server/restart", [api_instance](const auto& params) {
            return api_instance->api_server_restart(params);
        });

        server.post("/api/server/shutdown", [api_instance](const auto& params) {
            return api_instance->api_server_shutdown(params);
        });

        server.post("/api/server/reload-config", [api_instance](const auto& params) {
            return api_instance->api_server_reload_config(params);
        });

        // 用户管理
        server.post("/api/users/kick-all", [api_instance](const auto& params) {
            return api_instance->api_kick_all_users(params);
        });

        server.post("/api/messages/broadcast", [api_instance](const auto& params) {
            return api_instance->api_send_broadcast(params);
        });

        server.post("/api/messages/clear-old", [api_instance](const auto& params) {
            return api_instance->api_clear_old_messages(params);
        });

        server.get("/api/messages/export", [api_instance](const auto& params) {
            return api_instance->api_export_messages(params);
        });

        // 系统监控
        server.get("/api/performance", [api_instance](const auto& params) {
            return api_instance->api_performance(params);
        });

        server.get("/api/logs", [api_instance](const auto& params) {
            return api_instance->api_logs(params);
        });

        // 数据库管理
        server.post("/api/database/backup", [api_instance](const auto& params) {
            return api_instance->api_database_backup(params);
        });

        LOG_INFO("WebAPI 路由注册完成，使用服务层架构");
    }
};