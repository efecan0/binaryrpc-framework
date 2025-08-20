/**
 * @file thread_pool.cpp
 * @brief Implementation of the ThreadPool class.
 *
 * @author Efecan
 * @date 2025
 */
#include "binaryrpc/core/util/thread_pool.hpp"
#include <algorithm>
#include <stdexcept>

namespace binaryrpc {

    ThreadPool::ThreadPool(size_t thread_count) 
        : stop_(false), pending_tasks_(0) {
        
        // If thread_count is 0, use hardware concurrency
        if (thread_count == 0) {
            thread_count = std::thread::hardware_concurrency();
            if (thread_count == 0) {
                thread_count = 2; // Fallback to 2 threads
            }
        }
        
        // Create worker threads
        for (size_t i = 0; i < thread_count; ++i) {
            threads_.emplace_back(&ThreadPool::workerFunction, this);
        }
    }

    ThreadPool::~ThreadPool() {
        join();
    }

    void ThreadPool::add(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            tasks_.emplace(std::move(task));
            ++pending_tasks_;
        }
        
        condition_.notify_one();
    }

    void ThreadPool::join() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        // Wait for all threads to finish
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        threads_.clear();
    }

    size_t ThreadPool::getPendingTaskCount() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return pending_tasks_;
    }

    void ThreadPool::workerFunction() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                
                // Wait for a task or stop signal
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });
                
                // If stopped and no tasks, exit
                if (stop_ && tasks_.empty()) {
                    return;
                }
                
                // Get the next task
                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    --pending_tasks_;
                }
            }
            
            // Execute the task
            if (task) {
                try {
                    task();
                } catch (...) {
                    // Task exceptions are not propagated to avoid thread pool corruption
                    // In a production environment, you might want to log these exceptions
                }
            }
        }
    }

} // namespace binaryrpc
