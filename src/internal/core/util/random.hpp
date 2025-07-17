/**
 * @file random.hpp
 * @brief Random number utility functions for BinaryRPC.
 *
 * Provides helper functions for generating cryptographically suitable random tokens,
 * typically used for session IDs and other unique identifiers.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <array>
#include <cstdint>
#include <random>

namespace binaryrpc {

    /**
     * @brief Fill a 16-byte array with cryptographically suitable random data.
     *
     * Uses a thread-local Mersenne Twister engine seeded with std::random_device and high-resolution clock.
     *
     * @param tok Reference to a 16-byte array to fill with random bytes
     */
    inline void randomFill(std::array<uint8_t, 16>& tok)
    {
        static thread_local std::mt19937_64 rng{
            std::random_device{}() ^ (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()
        };
        std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

        for (size_t i = 0; i < 16; i += 4) {
            uint32_t rnd = dist(rng);
            tok[i] = static_cast<uint8_t>(rnd & 0xFF);
            tok[i + 1] = static_cast<uint8_t>((rnd >> 8) & 0xFF);
            tok[i + 2] = static_cast<uint8_t>((rnd >> 16) & 0xFF);
            tok[i + 3] = static_cast<uint8_t>((rnd >> 24) & 0xFF);
        }
    }

}