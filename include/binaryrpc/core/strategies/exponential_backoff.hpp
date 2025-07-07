/**
 * @file exponential_backoff.hpp
 * @brief Exponential backoff strategy implementation for BinaryRPC.
 *
 * Provides an implementation of IBackoffStrategy that increases the delay exponentially
 * with each retry attempt, up to a maximum delay.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "../interfaces/IBackoffStrategy.hpp"
#include <algorithm>

namespace binaryrpc {

    /**
     * @class ExponentialBackoff
     * @brief Exponential backoff strategy for retrying message delivery.
     *
     * Increases the delay exponentially with each retry attempt, up to a maximum delay.
     */
    class ExponentialBackoff : public IBackoffStrategy {
    public:
        /**
         * @brief Construct an ExponentialBackoff strategy.
         * @param base Initial delay for the first retry
         * @param max Maximum delay for any retry
         */
        ExponentialBackoff(std::chrono::milliseconds base, std::chrono::milliseconds max)
            : base_(base), max_(max) {}

        /**
         * @brief Calculate the next backoff delay based on the attempt number.
         * @param attempt Current retry attempt (starting from 1)
         * @return Duration to wait before the next retry
         */
        std::chrono::milliseconds nextDelay(uint32_t attempt) const override {
            long long delay = (long long)base_.count() * (1LL << (attempt - 1));
            delay = std::min(delay, (long long)max_.count());
            return std::chrono::milliseconds(delay);
        }

    private:
        std::chrono::milliseconds base_;
        std::chrono::milliseconds max_;
    };

}