/**
 * @file duplicate_filter.hpp
 * @brief DuplicateFilter for QoS-1 deduplication in BinaryRPC.
 *
 * Provides a deduplication filter for RPC calls, preventing duplicate processing
 * of the same payload within a configurable TTL window.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <unordered_set>
#include <deque>
#include <chrono>
#include <vector>
#include <string>

namespace binaryrpc::qos {

    /**
     * @class DuplicateFilter
     * @brief Deduplication filter for QoS-1 RPC calls.
     *
     * Tracks recently seen RPC payloads and prevents duplicate processing within a TTL window.
     * Used to ensure at-least-once delivery semantics without duplicate side effects.
     */
    class DuplicateFilter {
    public:
        /**
         * @brief Accept or reject an RPC call based on recent duplicates.
         *
         * @param rpcPayload The payload of the RPC call (e.g., "counter:inc", "abc:inc")
         * @param ttl If the same payload is received within this duration, it is considered a duplicate
         * @return true  → RPC call is new or TTL has expired
         *         false → Duplicate RPC call (within TTL window)
         */
        bool accept(const std::vector<uint8_t>& rpcPayload,
            std::chrono::milliseconds ttl = std::chrono::milliseconds(1000)) // 1 second default
        {
            using clock = std::chrono::steady_clock;
            const auto now = clock::now();

            // Convert RPC payload to string and hash it
            std::string rpcStr(rpcPayload.begin(), rpcPayload.end());
            auto rpcHash = std::hash<std::string>{}(rpcStr);

            // Remove expired RPC entries from the TTL window
            while (!order_.empty() && now - order_.front().second > ttl) {
                seen_.erase(order_.front().first);
                order_.pop_front();
            }

            // Has this RPC been called before?
            auto it = seen_.find(rpcHash);
            if (it == seen_.end()) {
                // New RPC call
                seen_.insert(rpcHash);
                order_.emplace_back(rpcHash, now);
                return true;
            }

            // Duplicate RPC call, check TTL
            for (const auto& [hash, timestamp] : order_) {
                if (hash == rpcHash) {
                    if (now - timestamp > ttl) {
                        // TTL expired → accept and refresh
                        seen_.erase(hash);
                        order_.erase(std::remove_if(order_.begin(), order_.end(),
                            [hash](const auto& pair) { return pair.first == hash; }),
                            order_.end());
                        seen_.insert(rpcHash);
                        order_.emplace_back(rpcHash, now);
                        return true;
                    }
                    return false; // Still within TTL → reject
                }
            }

            return true; // Should not reach here, but return true for safety
        }

    private:
        std::unordered_set<size_t> seen_;  ///< Set of hashes for deduplication
        std::deque<std::pair<size_t, std::chrono::steady_clock::time_point>> order_;  ///< Hash and timestamp pairs for TTL tracking
        static constexpr std::size_t WINDOW = 2048;   ///< Memory window limit (approx. 16KB)
    };

}