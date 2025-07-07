/**
 * @file conn_state.hpp
 * @brief Connection state and QoS buffer structures for BinaryRPC sessions.
 *
 * Defines data structures for managing per-connection state, including QoS-1 and QoS-2 message buffers,
 * retry logic, duplicate detection, and statistics for BinaryRPC sessions.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <folly/SharedMutex.h>
#include <folly/container/F14Map.h>
#include <folly/container/F14Set.h>

namespace binaryrpc {

    /**
     * @struct FrameInfo
     * @brief Stores information about a single QoS-1 frame, including retry state.
     */
    struct FrameInfo {
        std::vector<uint8_t>            frame;      ///< The message frame data
        std::chrono::steady_clock::time_point nextRetry{}; ///< Next retry time point
        uint32_t                        retryCount{ 0 };   ///< Number of retries attempted
    };

    /**
     * @struct Q2Meta
     * @brief Metadata for QoS-2 (ExactlyOnce) message state and retry logic.
     */
    struct Q2Meta {
        /**
         * @enum Stage
         * @brief Stages of the QoS-2 two-phase commit protocol.
         */
        enum class Stage : uint8_t { PREPARE, COMMIT };
        Stage                               stage{Stage::PREPARE}; ///< Current stage
        std::vector<uint8_t>                frame;       ///< Frame to be resent
        uint32_t                            retryCount{0}; ///< Number of retries attempted
        std::chrono::steady_clock::time_point nextRetry{}; ///< Next retry time point
        std::chrono::steady_clock::time_point lastTouched{}; ///< Last activity time point
    };

    /**
     * @struct ConnState
     * @brief Per-connection state for QoS message management and duplicate detection.
     *
     * Holds buffers and state for QoS-1 and QoS-2 message delivery, duplicate detection,
     * and statistics for a single client connection.
     */
    struct ConnState {
        /*──────── QoS-1 ────────*/
        std::atomic_uint64_t                 nextId{ 1 }; ///< Next message ID for QoS-1
        folly::F14FastMap<uint64_t, FrameInfo> pending1; ///< Pending QoS-1 frames
        folly::SharedMutex                   pendMx;     ///< Mutex for QoS-1 pending frames

        /* "gördüm" kümesi */
        folly::F14FastSet<uint64_t>         seenSet;     ///< Set of seen message IDs (duplicate detection)
        std::deque<std::pair<uint64_t, std::chrono::steady_clock::time_point>> seenQ; ///< Queue of seen IDs and timestamps

        /*──────── QoS-2 ────────*/
        folly::F14FastMap<uint64_t, std::vector<uint8_t>> pubPrepare;  ///< PREPARE state frames
        folly::F14FastMap<uint64_t, std::vector<uint8_t>> pendingResp; ///< COMMIT state frames
        folly::F14FastMap<uint64_t, Q2Meta> qos2Pending;               ///< QoS-2 metadata
        folly::SharedMutex                   q2Mx;        ///< Mutex for QoS-2 state

        /* İstatistik / quota için toplam bayt */
        std::size_t queuedBytes{ 0 }; ///< Total bytes queued for statistics/quota
    };

}