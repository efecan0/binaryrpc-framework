/**
 * @file IBackoffStrategy.hpp
 * @brief Interface for custom retry backoff strategies in BinaryRPC.
 *
 * Defines the IBackoffStrategy interface for implementing custom retry backoff algorithms
 * used in reliable message delivery (QoS) scenarios.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <chrono>

namespace binaryrpc {

    /**
     * @class IBackoffStrategy
     * @brief Interface for custom retry backoff strategies.
     *
     * Implement this interface to provide custom backoff algorithms for retrying message delivery.
     */
    class IBackoffStrategy {
    public:
        virtual ~IBackoffStrategy() = default;

        /**
         * @brief Calculate the next backoff delay.
         * @param attempt Current retry attempt (starting from 1)
         * @return Duration to wait before the next retry
         */
        virtual std::chrono::milliseconds nextDelay(uint32_t attempt) const = 0;
    };

}