#pragma once
#include "server_command.hpp"
#include <memory>
#include <unordered_map>

class CommandFactory {
private:
    std::unordered_map<std::string, std::shared_ptr<ServerCommand>> commands_;

public:
    CommandFactory() {
        register_commands();
    }

    void register_commands() {
        commands_["restart"] = std::make_shared<RestartServerCommand>();
        commands_["shutdown"] = std::make_shared<ShutdownServerCommand>();
        commands_["reload-config"] = std::make_shared<ReloadConfigCommand>();
        commands_["database-backup"] = std::make_shared<DatabaseBackupCommand>();
    }

    std::shared_ptr<ServerCommand> create_command(const std::string& command_name) {
        auto it = commands_.find(command_name);
        if (it != commands_.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool command_exists(const std::string& command_name) const {
        return commands_.find(command_name) != commands_.end();
    }
};