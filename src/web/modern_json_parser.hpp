#pragma once
#include <string>
#include <map>
#include <variant>
#include <optional>
#include <string_view>

class ModernJsonParser {
public:
    // JSON 值类型定义
    using JsonValue = std::variant<std::string, double, bool, std::nullptr_t>;
    using JsonObject = std::map<std::string, JsonValue>;
    
    /**
     * @brief 解析 JSON 字符串
     * @param json_str 要解析的 JSON 字符串
     * @return 解析成功的 JsonObject，如果解析失败返回 std::nullopt
     */
    static std::optional<JsonObject> parse(const std::string& json_str);
    
    /**
     * @brief 解析 JSON 字符串视图（更高效的版本）
     * @param json_str 要解析的 JSON 字符串视图
     * @return 解析成功的 JsonObject，如果解析失败返回 std::nullopt
     */
    static std::optional<JsonObject> parse(std::string_view json_str);
    
    /**
     * @brief 将 JsonValue 转换为字符串表示
     * @param value 要转换的 JSON 值
     * @return 值的字符串表示
     */
    static std::string value_to_string(const JsonValue& value);
    
    /**
     * @brief 检查字符串是否为有效的 JSON 对象
     * @param json_str 要检查的字符串
     * @return 如果是有效的 JSON 对象返回 true，否则返回 false
     */
    static bool is_valid_json(const std::string& json_str);

private:
    // 工具函数
    static std::string_view trim(std::string_view str);
    static size_t skip_whitespace(std::string_view str, size_t pos);
    
    // 解析函数
    static std::optional<std::pair<std::string, JsonValue>> parse_key_value(std::string_view str, size_t& pos);
    static std::optional<std::string> parse_string(std::string_view str, size_t& pos);
    static std::optional<JsonValue> parse_value(std::string_view str, size_t& pos);
    static std::optional<JsonValue> parse_number(std::string_view str, size_t& pos);
    static std::optional<JsonValue> parse_literal(std::string_view str, size_t& pos);
    
    // 字符串处理
    static std::string unescape_string(const std::string& str);
    
    // 错误处理
    static void log_parse_error(std::string_view context, size_t position, char unexpected_char);
};