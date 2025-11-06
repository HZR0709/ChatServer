#pragma once
#include "IDatabase.hpp"

class ISessionRepository {
public:
    virtual ~ISessionRepository() = default;
    
    virtual bool create_session(int user_id, int socket_fd, const std::string& ip_address = "") = 0;
    virtual bool update_session_activity(int socket_fd) = 0;
    virtual bool delete_session(int socket_fd) = 0;
    virtual bool delete_all_sessions() = 0;
    
    // 查询接口
    virtual int get_user_id_by_socket(int socket_fd) = 0;
    virtual std::vector<int> get_user_sessions(int user_id) = 0;
};