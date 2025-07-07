/**
 * @file qos.hpp
 * @brief Quality of Service (QoS) level definitions and reliability options for BinaryRPC.
 *
 * Provides enums and configuration structures for controlling message delivery guarantees,
 * retry strategies, and reliability options in the BinaryRPC framework.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "../interfaces/IBackoffStrategy.hpp"

namespace binaryrpc {

    /**
     * @enum QoSLevel
     * @brief Defines the quality of service (QoS) levels for message delivery.
     *
     * - None: At most once delivery (no ACK expected)
     * - AtLeastOnce: At least once delivery (ACK and retry)
     * - ExactlyOnce: Exactly once delivery (two-phase commit)
     */
    enum class QoSLevel {
        None = 0,        ///< QoS 0 – "at most once" (no ACK expected)
        AtLeastOnce = 1, ///< QoS 1 – ACK and retry
        ExactlyOnce = 2  ///< QoS 2 – exactly once delivery
    };

    /**
     * @struct ReliableOptions
     * @brief Configuration options for reliable message delivery.
     *
     * Used with setReliable() to control retry behavior, backoff strategy, session TTL, and more.
     */
    struct ReliableOptions {
        QoSLevel level{ QoSLevel::None };           ///< Desired QoS level
        uint32_t   baseRetryMs{ 100 };              ///< Initial retry delay in milliseconds
        uint32_t   maxRetry{ 3 };                   ///< Maximum number of retries
        uint32_t   maxBackoffMs{ 1000 };            ///< Maximum backoff delay in milliseconds
        uint64_t   sessionTtlMs  { 15 * 60 * 1000 };///< Session time-to-live in milliseconds
        uint32_t   duplicateTtlMs{ 5000 };          ///< TTL for duplicate detection (ms)
        std::shared_ptr<IBackoffStrategy> backoffStrategy; ///< Custom backoff strategy
        bool enableCompression{false};              ///< Enable message compression
        size_t compressionThresholdBytes{1024};     ///< Minimum size for compression
        size_t maxSendQueueSize{1000};              ///< Maximum send queue size per session
    };


}
