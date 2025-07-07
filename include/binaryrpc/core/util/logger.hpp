/**
 * @file logger.hpp
 * @brief Logging utilities for BinaryRPC.
 *
 * Provides a singleton Logger class and logging macros for different log levels.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <chrono>
#include <iostream>

namespace binaryrpc {

    /**
     * @enum LogLevel
     * @brief Log levels for the logger.
     */
    enum class LogLevel { Trace, Debug, Info, Warn, Error };

    /**
     * @class Logger
     * @brief Singleton logger class for BinaryRPC.
     *
     * Provides thread-safe logging with customizable log sinks and log levels.
     */
    class Logger {
    public:
        using Sink = std::function<void(LogLevel, const std::string&)>;

        /**
         * @brief Get the singleton Logger instance.
         * @return Reference to the Logger instance
         */
        static Logger& inst() {
            static Logger L;  return L;
        }

        /**
         * @brief Set the minimum log level.
         * @param lvl LogLevel to set
         */
        void setLevel(LogLevel lvl) { level_ = lvl; }
        /**
         * @brief Set a custom log sink function.
         * @param s Sink function to use
         */
        void setSink(Sink s) { sink_ = std::move(s); }

        /**
         * @brief Log a message at the specified log level.
         * @param lvl LogLevel for the message
         * @param msg Message to log
         */
        void log(LogLevel lvl, const std::string& msg) {
            if (lvl < level_) return;
            std::scoped_lock lk(m_);
            sink_(lvl, msg);
        }

    private:
        Logger() {
            /* default sink → stdout */
            sink_ = [](LogLevel l, const std::string& m) {
                static const char* names[]{ "TRACE","DEBUG","INFO","WARN","ERROR" };
                std::cout << "[" << names[(int)l] << "] " << m << '\n';
            };
        }
        std::mutex m_;
        LogLevel   level_{ LogLevel::Info };
        Sink       sink_;
    };

    /**
     * @def LOG_TRACE
     * @brief Log a message at TRACE level.
     */
    /**
     * @def LOG_DEBUG
     * @brief Log a message at DEBUG level.
     */
    /**
     * @def LOG_INFO
     * @brief Log a message at INFO level.
     */
    /**
     * @def LOG_WARN
     * @brief Log a message at WARN level.
     */
    /**
     * @def LOG_ERROR
     * @brief Log a message at ERROR level.
     */
#define LOG_TRACE(msg) ::binaryrpc::Logger::inst().log(::binaryrpc::LogLevel::Trace, msg)
#define LOG_DEBUG(msg) ::binaryrpc::Logger::inst().log(::binaryrpc::LogLevel::Debug, msg)
#define LOG_INFO(msg)  ::binaryrpc::Logger::inst().log(::binaryrpc::LogLevel::Info,  msg)
#define LOG_WARN(msg)  ::binaryrpc::Logger::inst().log(::binaryrpc::LogLevel::Warn,  msg)
#define LOG_ERROR(msg) ::binaryrpc::Logger::inst().log(::binaryrpc::LogLevel::Error, msg)
}