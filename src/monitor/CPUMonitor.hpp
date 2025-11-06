#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

class CPUMonitor {
private:
    struct CPUData {
        unsigned long long user;
        unsigned long long nice;
        unsigned long long system;
        unsigned long long idle;
        unsigned long long iowait;
        unsigned long long irq;
        unsigned long long softirq;
        unsigned long long steal;
        unsigned long long guest;
        unsigned long long guest_nice;
        
        unsigned long long total() const {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }
        
        unsigned long long idle_time() const {
            return idle + iowait;
        }
    };
    
    CPUData previous_cpu_data_;
    bool first_measurement_ = true;

public:
    // 获取当前CPU数据
    CPUData get_cpu_data();
    
    // 计算CPU使用率（百分比）
    double get_cpu_usage();
    
    // 获取每个核心的使用率
    std::vector<double> get_per_core_usage();
};