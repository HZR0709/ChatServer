#pragma once

#include <string>
#include <map>
#include "web/web_server.hpp"

class WebAPI {
public:
    static void register_routes(WebServer& server);
    
private:
    static std::string api_stats(const std::map<std::string, std::string>& params);
    static std::string api_users(const std::map<std::string, std::string>& params);
    static std::string api_messages(const std::map<std::string, std::string>& params);
    static std::string api_sessions(const std::map<std::string, std::string>& params);
    static std::string api_system_info(const std::map<std::string, std::string>& params);
    
    static std::string json_response(const std::string& data, bool success = true, const std::string& message = "");
    static std::string error_response(const std::string& message);
};