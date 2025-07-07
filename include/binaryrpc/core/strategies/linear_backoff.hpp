/**
 * @file linear_backoff.hpp
 * @brief Linear backoff strategy implementation for BinaryRPC.
 *
 * Provides an implementation of IBackoffStrategy that increases the delay linearly
 * with each retry attempt, up to a maximum delay.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "../interfaces/IBackoffStrategy.hpp"
#include <chrono>

namespace binaryrpc {

    /**
     * @class LinearBackoff
     * @brief Linear backoff strategy for retrying message delivery.
     *
     * Increases the delay linearly with each retry attempt, up to a maximum delay.
     */
    class LinearBackoff : public IBackoffStrategy {
    public:
        /**
         * @brief Construct a LinearBackoff strategy.
         * @param base Initial delay for the first retry
         * @param max Maximum delay for any retry
         */
        LinearBackoff(std::chrono::milliseconds base, std::chrono::milliseconds max)
            : base_(base), max_(max) {}

        /**
         * @brief Calculate the next backoff delay based on the attempt number.
         * @param attempt Current retry attempt (starting from 1)
         * @return Duration to wait before the next retry
         */
        std::chrono::milliseconds nextDelay(uint32_t attempt) const override {
            auto d = base_ * attempt;
            return d < max_ ? d : max_;
        }
    private:
        std::chrono::milliseconds base_;
        std::chrono::milliseconds max_;
    };

} // namespace binaryrpc
