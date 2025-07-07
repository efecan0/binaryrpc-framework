/**
 * @file time.hpp
 * @brief Time utility functions for BinaryRPC.
 *
 * Provides helper functions for retrieving the current time in milliseconds,
 * using both wall-clock and steady clock sources.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <chrono>
#include <cstdint>

namespace binaryrpc {

    /**
     * @brief Get the current wall-clock time in milliseconds since the Unix epoch.
     *
     * Uses std::chrono::system_clock to retrieve the current time.
     *
     * @return Current time in milliseconds since epoch (uint64_t)
     */
    inline std::uint64_t epochMillis()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief Get the current steady clock time in milliseconds.
     *
     * Uses std::chrono::steady_clock for monotonic time measurement.
     *
     * @return Current steady clock time in milliseconds (uint64_t)
     */
    static uint64_t clockMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()
            ).count();
    }
}