#pragma once
#include <string>
#include <optional>
#include <variant>

template<typename T = void>
class ServiceResult {
private:
    bool success_;
    std::string message_;
    std::optional<T> data_;
    int error_code_;

    // 私有构造函数，通过工厂方法创建
    ServiceResult(bool success, std::string message, std::optional<T> data, int error_code)
        : success_(success), message_(std::move(message)), 
          data_(std::move(data)), error_code_(error_code) {}

public:
    // 工厂方法 - 成功结果（有数据）
    static ServiceResult<T> success(T data, std::string message = "") {
        return ServiceResult(true, std::move(message), std::move(data), 0);
    }
    
    // 工厂方法 - 失败结果
    static ServiceResult<T> failure(std::string message, int error_code = 0) {
        return ServiceResult(false, std::move(message), std::nullopt, error_code);
    }
    
    // 工厂方法 - 从其他类型转换
    template<typename U>
    static ServiceResult<T> from(ServiceResult<U>&& other) {
        if (other.is_success()) {
            if constexpr (std::is_convertible_v<U, T>) {
                return success(static_cast<T>(other.unwrap()), other.message());
            } else {
                return failure("类型转换失败", 500);
            }
        } else {
            return failure(other.message(), other.error_code());
        }
    }

    bool is_success() const { return success_; }
    const std::string& message() const { return message_; }
    const std::optional<T>& data() const { return data_; }
    int error_code() const { return error_code_; }
    
    /*unwrap 方法的作用是：如果 ServiceResult 对象表示一个成功的结果
    （即 success_ 为 true 并且 data_ 有值），则返回包含的数据；如果失败
    或没有数据，则抛出一个异常，异常信息会包含失败的原因。*/
    T unwrap() const { 
        if (!success_ || !data_) {
            throw std::runtime_error("Attempted to unwrap failed result: " + message_);
        }
        return *data_; 
    }
    
    const T& get_data() const {
        if (!success_ || !data_) {
            throw std::runtime_error("No data available: " + message_);
        }
        return *data_;
    }
    
    // 映射操作：将成功的结果转换为另一种类型
    template<typename F>
    auto map(F&& func) const -> ServiceResult<decltype(func(std::declval<T>()))> {
        using ResultType = decltype(func(std::declval<T>()));
        
        if (!success_) {
            return ServiceResult<ResultType>::failure(message_, error_code_);
        }
        
        try {
            return ServiceResult<ResultType>::success(func(*data_), message_);
        } catch (const std::exception& e) {
            return ServiceResult<ResultType>::failure(
                std::string("映射操作失败: ") + e.what(), 500);
        }
    }
    
    // 扁平映射操作：用于链式调用
    template<typename F>
    auto flat_map(F&& func) const -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));
        
        if (!success_) {
            return ResultType::failure(message_, error_code_);
        }
        
        try {
            return func(*data_);
        } catch (const std::exception& e) {
            return ResultType::failure(
                std::string("扁平映射操作失败: ") + e.what(), 500);
        }
    }
    
    // 处理失败情况
    template<typename F>
    ServiceResult<T> on_failure(F&& handler) const {
        if (!success_) {
            handler(message_, error_code_);
        }
        return *this;
    }
    
    // 处理成功情况
    template<typename F>
    ServiceResult<T> on_success(F&& handler) const {
        if (success_ && data_) {
            handler(*data_);
        }
        return *this;
    }
};

// void 特化版本
template<>
class ServiceResult<void> {
private:
    bool success_;
    std::string message_;
    int error_code_;

    ServiceResult(bool success, std::string message, int error_code)
        : success_(success), message_(std::move(message)), error_code_(error_code) {}

public:
    static ServiceResult<void> success(std::string message = "") {
        return ServiceResult(true, std::move(message), 0);
    }
    
    static ServiceResult<void> failure(std::string message, int error_code = 0) {
        return ServiceResult(false, std::move(message), error_code);
    }

    bool is_success() const { return success_; }
    const std::string& message() const { return message_; }
    int error_code() const { return error_code_; }
    
    void unwrap() const {
        if (!success_) {
            throw std::runtime_error("Attempted to unwrap failed result: " + message_);
        }
    }
    
    template<typename F>
    ServiceResult<void> on_failure(F&& handler) const {
        if (!success_) {
            handler(message_, error_code_);
        }
        return *this;
    }
    
    template<typename F>
    ServiceResult<void> on_success(F&& handler) const {
        if (success_) {
            handler();
        }
        return *this;
    }
    
    // 将 void 结果转换为有值结果
    template<typename U>
    ServiceResult<U> to_value(U value) const {
        if (success_) {
            return ServiceResult<U>::success(std::move(value), message_);
        } else {
            return ServiceResult<U>::failure(message_, error_code_);
        }
    }
};