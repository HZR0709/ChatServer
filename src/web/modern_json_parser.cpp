#include "web/modern_json_parser.hpp"
#include <iostream>
#include <cctype>
#include <charconv>
#include <cmath>

// 解析字符串版本的 JSON
std::optional<ModernJsonParser::JsonObject> ModernJsonParser::parse(const std::string& json_str) {
    return parse(std::string_view(json_str));
}

// 解析 string_view 版本的 JSON（主要实现）
std::optional<ModernJsonParser::JsonObject> ModernJsonParser::parse(std::string_view json_str) {
    std::string_view trimmed = trim(json_str);
    
    // 基本格式验证
    if (trimmed.empty()) {
        std::cerr << "JSON解析错误: 输入为空" << std::endl;
        return std::nullopt;
    }
    
    if (trimmed.front() != '{' || trimmed.back() != '}') {
        std::cerr << "JSON解析错误: 不是有效的JSON对象格式" << std::endl;
        return std::nullopt;
    }
    
    JsonObject result;
    std::string_view content = trimmed.substr(1, trimmed.length() - 2);
    size_t pos = 0;
    
    // 解析所有键值对
    while (pos < content.length()) {
        pos = skip_whitespace(content, pos);
        if (pos >= content.length()) break;
        
        // 如果遇到结束符，提前退出
        if (content[pos] == '}') break;
        
        auto pair_opt = parse_key_value(content, pos);
        if (!pair_opt) {
            // 解析失败
            return std::nullopt;
        }
        
        auto [key, value] = pair_opt.value();
        result[std::string(key)] = value;
        
        // 跳过逗号，准备解析下一个键值对
        pos = skip_whitespace(content, pos);
        if (pos < content.length() && content[pos] == ',') {
            pos++;
        }
    }
    
    return result;
}

// 将 JsonValue 转换为字符串
std::string ModernJsonParser::value_to_string(const JsonValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, double>) {
            // 避免科学计数法，优化浮点数显示
            std::string str = std::to_string(arg);
            
            // 移除不必要的尾随零和小数点
            size_t dot_pos = str.find('.');
            if (dot_pos != std::string::npos) {
                // 找到最后一个非零数字的位置
                size_t last_non_zero = str.find_last_not_of('0');
                if (last_non_zero != std::string::npos) {
                    if (last_non_zero == dot_pos) {
                        // 只有小数点，没有小数部分
                        str = str.substr(0, dot_pos);
                    } else {
                        str = str.substr(0, last_non_zero + 1);
                    }
                }
            }
            return str;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "null";
        } else {
            return "unknown";
        }
    }, value);
}

// 检查是否为有效的 JSON
bool ModernJsonParser::is_valid_json(const std::string& json_str) {
    auto result = parse(json_str);
    return result.has_value();
}

// ============ 私有工具函数实现 ============

std::string_view ModernJsonParser::trim(std::string_view str) {
    size_t start = 0;
    while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    
    if (start >= str.length()) return "";
    
    size_t end = str.length() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }
    
    return str.substr(start, end - start + 1);
}

size_t ModernJsonParser::skip_whitespace(std::string_view str, size_t pos) {
    while (pos < str.length() && std::isspace(static_cast<unsigned char>(str[pos]))) {
        pos++;
    }
    return pos;
}

std::optional<std::pair<std::string, ModernJsonParser::JsonValue>> 
ModernJsonParser::parse_key_value(std::string_view str, size_t& pos) {
    // 解析键
    auto key_opt = parse_string(str, pos);
    if (!key_opt) {
        log_parse_error("期望键", pos, str[pos]);
        return std::nullopt;
    }
    
    pos = skip_whitespace(str, pos);
    
    // 检查冒号分隔符
    if (pos >= str.length() || str[pos] != ':') {
        log_parse_error("期望冒号", pos, str[pos]);
        return std::nullopt;
    }
    pos++;
    
    pos = skip_whitespace(str, pos);
    
    // 解析值
    auto value_opt = parse_value(str, pos);
    if (!value_opt) {
        log_parse_error("期望值", pos, str[pos]);
        return std::nullopt;
    }
    
    return std::make_pair(key_opt.value(), value_opt.value());
}

std::optional<std::string> ModernJsonParser::parse_string(std::string_view str, size_t& pos) {
    if (pos >= str.length() || str[pos] != '"') {
        return std::nullopt;
    }
    
    pos++; // 跳过开头的引号
    size_t start = pos;
    bool escape = false;
    
    while (pos < str.length()) {
        if (escape) {
            escape = false;
            pos++;
            continue;
        }
        
        if (str[pos] == '\\') {
            escape = true;
            pos++;
        } else if (str[pos] == '"') {
            // 找到字符串结束位置
            std::string raw_result = std::string(str.substr(start, pos - start));
            pos++; // 跳过结尾的引号
            
            // 处理转义字符
            return unescape_string(raw_result);
        } else {
            pos++;
        }
    }
    
    // 没有找到结尾的引号
    std::cerr << "JSON解析错误: 未找到字符串的结束引号" << std::endl;
    return std::nullopt;
}

std::optional<ModernJsonParser::JsonValue> ModernJsonParser::parse_value(std::string_view str, size_t& pos) {
    pos = skip_whitespace(str, pos);
    if (pos >= str.length()) {
        return std::nullopt;
    }
    
    char c = str[pos];
    
    if (c == '"') {
        // 字符串值
        return parse_string(str, pos);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        // 数字值
        return parse_number(str, pos);
    } else {
        // 字面量值（true, false, null）
        return parse_literal(str, pos);
    }
}

std::optional<ModernJsonParser::JsonValue> ModernJsonParser::parse_number(std::string_view str, size_t& pos) {
    size_t start = pos;
    
    // 处理符号
    if (str[pos] == '-') {
        pos++;
    }
    
    // 处理整数部分
    while (pos < str.length() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
        pos++;
    }
    
    // 处理小数部分
    if (pos < str.length() && str[pos] == '.') {
        pos++;
        while (pos < str.length() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
            pos++;
        }
    }
    
    // 处理指数部分
    if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
        pos++;
        if (pos < str.length() && (str[pos] == '+' || str[pos] == '-')) {
            pos++;
        }
        while (pos < str.length() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
            pos++;
        }
    }
    
    std::string_view num_str = str.substr(start, pos - start);
    
    // 使用 std::from_chars 进行高效的数字转换
    double value;
    auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.length(), value);
    
    if (ec == std::errc()) {
        return value;
    } else {
        std::cerr << "JSON解析错误: 数字格式无效: " << num_str << std::endl;
        return std::nullopt;
    }
}

std::optional<ModernJsonParser::JsonValue> ModernJsonParser::parse_literal(std::string_view str, size_t& pos) {
    // 检查 true
    if (pos + 4 <= str.length() && str.substr(pos, 4) == "true") {
        pos += 4;
        return true;
    }
    
    // 检查 false
    if (pos + 5 <= str.length() && str.substr(pos, 5) == "false") {
        pos += 5;
        return false;
    }
    
    // 检查 null
    if (pos + 4 <= str.length() && str.substr(pos, 4) == "null") {
        pos += 4;
        return nullptr;
    }
    
    std::cerr << "JSON解析错误: 未知的字面量值" << std::endl;
    return std::nullopt;
}

std::string ModernJsonParser::unescape_string(const std::string& str) {
    std::string result;
    result.reserve(str.length()); // 预分配空间
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            // 处理转义序列
            switch (str[i + 1]) {
                case '"':  result += '"';  i++; break;
                case '\\': result += '\\'; i++; break;
                case '/':  result += '/';  i++; break;
                case 'b':  result += '\b'; i++; break;
                case 'f':  result += '\f'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                case 'u': 
                    // 简化处理 Unicode 转义，用 '?' 代替
                    if (i + 5 < str.length()) {
                        i += 5;
                        result += '?';
                    } else {
                        result += str[i];
                    }
                    break;
                default:
                    // 未知转义序列，保持原样
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

void ModernJsonParser::log_parse_error(std::string_view context, size_t position, char unexpected_char) {
    std::cerr << "JSON解析错误: " << context 
              << " 在位置 " << position 
              << "，遇到意外的字符: '" 
              << (unexpected_char == '\0' ? "EOF" : std::string(1, unexpected_char)) 
              << "'" << std::endl;
}