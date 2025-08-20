/**
 * @file thread_pool.hpp
 * @brief Simple thread pool implementation to replace Folly's CPUThreadPoolExecutor.
 *
 * Provides a lightweight, efficient thread pool for asynchronous task execution.
 * Designed to be a drop-in replacement for Folly's thread pool functionality.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

namespace binaryrpc {

    /**
     * @class ThreadPool
     * @brief Simple thread pool implementation for asynchronous task execution.
     *
     * This class provides a thread pool that can execute tasks asynchronously.
     * It's designed to be a lightweight replacement for Folly's CPUThreadPoolExecutor.
     */
    class ThreadPool {
    public:
        /**
         * @brief Constructor for ThreadPool.
         * @param thread_count Number of worker threads to create. If 0, uses hardware concurrency.
         */
        explicit ThreadPool(size_t thread_count = 0);

        /**
         * @brief Destructor for ThreadPool.
         * 
         * Waits for all threads to finish and joins them.
         */
        ~ThreadPool();

        // Delete copy constructor and assignment operator
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /**
         * @brief Add a task to the thread pool.
         * @param task Function to execute
         * @return Future containing the result of the task
         */
        template<typename F, typename... Args>
        auto add(F&& task, Args&&... args) -> std::future<decltype(task(args...))>;

        /**
         * @brief Add a task to the thread pool (void return type).
         * @param task Function to execute
         */
        void add(std::function<void()> task);

        /**
         * @brief Wait for all tasks to complete.
         */
        void join();

        /**
         * @brief Get the number of worker threads.
         * @return Number of worker threads
         */
        size_t getThreadCount() const { return threads_.size(); }

        /**
         * @brief Get the number of pending tasks.
         * @return Number of pending tasks
         */
        size_t getPendingTaskCount() const;

    private:
        /**
         * @brief Worker thread function.
         */
        void workerFunction();

        std::vector<std::thread> threads_;                    ///< Worker threads
        std::queue<std::function<void()>> tasks_;             ///< Task queue
        mutable std::mutex queue_mutex_;                      ///< Mutex for task queue
        std::condition_variable condition_;                   ///< Condition variable for task signaling
        std::atomic<bool> stop_;                              ///< Stop flag
        std::atomic<size_t> pending_tasks_;                   ///< Number of pending tasks
    };

    // Template implementation for add method
    template<typename F, typename... Args>
    auto ThreadPool::add(F&& task, Args&&... args) -> std::future<decltype(task(args...))> {
        using return_type = decltype(task(args...));
        
        auto packaged_task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(task), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = packaged_task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            tasks_.emplace([packaged_task]() { (*packaged_task)(); });
            ++pending_tasks_;
        }
        
        condition_.notify_one();
        return result;
    }

} // namespace binaryrpc
