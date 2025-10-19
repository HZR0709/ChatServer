#pragma once

#include <string>
#include <vector>

// 常用的工具函数声明
std::vector<std::string> split_string(const std::string& str, char delimiter);
std::string trim(const std::string& str);
bool starts_with(const std::string& str, const std::string& prefix);
bool ends_with(const std::string& str, const std::string& suffix);