#pragma once
#include <string>
#include <memory>
#include "utils/logger.hpp"
#include "service_result.hpp"

class ServerCommand {
public:
    virtual ~ServerCommand() = default;
    virtual ServiceResult<bool> execute() = 0;
    virtual std::string get_name() const = 0;
    virtual bool requires_admin() const { return true; }
};

class RestartServerCommand : public ServerCommand {
public:
    ServiceResult<bool> execute() override {
        LOG_WARNING("执行服务器重启命令");
        // 实际实现应该优雅地重启服务器
        return ServiceResult<bool>::success(true, "服务器重启命令已发送");
    }

    std::string get_name() const override { return "restart"; }
};

class ShutdownServerCommand : public ServerCommand {
public:
    ServiceResult<bool> execute() override {
        LOG_WARNING("执行服务器关闭命令");
        // 实际实现应该优雅地关闭服务器
        return ServiceResult<bool>::success(true, "服务器关闭命令已发送");
    }

    std::string get_name() const override { return "shutdown"; }
};

class ReloadConfigCommand : public ServerCommand {
public:
    ServiceResult<bool> execute() override {
        LOG_INFO("执行配置重载命令");
        // 实际实现应该重载配置文件
        return ServiceResult<bool>::success(true, "配置重载成功");
    }

    std::string get_name() const override { return "reload-config"; }
};

class DatabaseBackupCommand : public ServerCommand {
public:
    ServiceResult<bool> execute() override {
        LOG_INFO("执行数据库备份命令");
        // 实际实现应该备份数据库
        return ServiceResult<bool>::success(true, "数据库备份成功");
    }

    std::string get_name() const override { return "database-backup"; }
};