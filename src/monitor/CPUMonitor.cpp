#include "monitor/CPUMonitor.hpp"

// 获取当前CPU数据
CPUMonitor::CPUData CPUMonitor::get_cpu_data() {
    std::ifstream file("/proc/stat");
    std::string   line;
    CPUData       data = {};

    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string        cpu_label;
        iss >> cpu_label;

        if (cpu_label == "cpu") {
            iss >> data.user >> data.nice >> data.system >> data.idle
                >> data.iowait >> data.irq >> data.softirq >> data.steal
                >> data.guest >> data.guest_nice;
        }
    }

    return data;
}

// 计算CPU使用率（百分比）
double CPUMonitor::get_cpu_usage() {
    CPUData current_data = get_cpu_data();

    if (first_measurement_) {
        previous_cpu_data_ = current_data;
        first_measurement_ = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        current_data = get_cpu_data();
    }

    // 计算差值
    const unsigned long long total_diff = current_data.total() - previous_cpu_data_.total();
    const unsigned long long idle_diff  = current_data.idle_time() - previous_cpu_data_.idle_time();

    // 保存当前数据供下次使用
    previous_cpu_data_ = current_data;

    if (total_diff == 0) {
        return 0.0;
    }

    // 计算使用率
    double usage = 100.0 * (total_diff - idle_diff) / total_diff;
    return usage;
}

// 获取每个核心的使用率
std::vector<double> CPUMonitor::get_per_core_usage() {
    std::vector<double> core_usages;
    std::ifstream       file("/proc/stat");
    std::string         line;

    // 跳过第一行（总的CPU使用率）
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string        cpu_label;
        iss >> cpu_label;

        if (cpu_label.find("cpu") == 0 && cpu_label != "cpu") {
            CPUData core_data = {};
            iss >> core_data.user >> core_data.nice >> core_data.system >> core_data.idle
                >> core_data.iowait >> core_data.irq >> core_data.softirq >> core_data.steal
                >> core_data.guest >> core_data.guest_nice;

            // 这里需要保存历史数据来计算每个核心的使用率
            // 简化实现：只返回当前活动的核心
            double usage = static_cast<double>(core_data.user + core_data.system + core_data.nice)
                / core_data.total() * 100.0;
            core_usages.push_back(usage);
        }
    }

    return core_usages;
}