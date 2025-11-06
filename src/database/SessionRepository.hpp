#pragma once
#include "ISessionRepository.hpp"
#include <memory>

class SessionRepository : public ISessionRepository {
public:
    explicit SessionRepository(std::shared_ptr<IDatabase> database);
    
    // ISessionRepository 接口实现
    bool create_session(int user_id, int socket_fd, const std::string& ip_address = "") override;
    bool update_session_activity(int socket_fd) override;
    bool delete_session(int socket_fd) override;
    bool delete_all_sessions() override;
    
    int get_user_id_by_socket(int socket_fd) override;
    std::vector<int> get_user_sessions(int user_id) override;
    
private:
    std::shared_ptr<IDatabase> database_;
};