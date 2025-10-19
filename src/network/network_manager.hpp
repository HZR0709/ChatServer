#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    bool initialize(int port);
    int accept_connection();
    bool send_message(int fd, const std::string& msg);
    std::string receive_message(int fd);
    void close_connection(int fd);
    int get_server_socket() const { return server_socket_; }
    void shutdown();

private:
    int server_socket_;
    struct sockaddr_in server_addr_;
    bool is_initialized_;
};