#include "thread/thread_pool.hpp"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    this->condition_.wait(lock, [this] {
                        return this->stop_ || !this->tasks_.empty();
                    });
                    
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }
                    
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                
                task();
            }
        });
    }
    
    std::cout << "线程池初始化完成，工作线程数: " << num_threads << std::endl;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "线程池已关闭" << std::endl;
}

size_t ThreadPool::get_task_count() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

size_t ThreadPool::get_thread_count() const {
    return workers_.size();
}