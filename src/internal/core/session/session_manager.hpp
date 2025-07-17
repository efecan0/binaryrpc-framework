/**
 * @file session_manager.hpp
 * @brief SessionManager class for managing client sessions in BinaryRPC.
 *
 * Provides creation, lookup, removal, and state management for client sessions,
 * including offline message queuing and TTL-based cleanup.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/auth/ClientIdentity.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include "internal/core/session/generic_index.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <format>
#include <vector>
#include "internal/core/util/random.hpp"
#include "internal/core/util/time.hpp"
#include <any>          // Generic state değerleri için
#include <cstring>
#include <queue>
#include <functional>
#include <optional> // For std::optional
#include <thread>   // For std::jthread

namespace binaryrpc {
    /**
     * @class SessionManager
     * @brief Manages client sessions, state, and offline message queues in BinaryRPC.
     *
     * Handles session creation, lookup, removal, state management, offline message queuing,
     * and TTL-based cleanup for all client sessions.
     */
    class SessionManager {
    public:
        /**
         * @brief Construct a SessionManager with a session TTL in milliseconds.
         * @param ttlMs Session time-to-live in milliseconds (default: 30,000 ms)
         */
        explicit SessionManager(std::uint64_t ttlMs = 30'000)  // 30 s default
            : ttlMs_{ ttlMs } {}

        /**
         * @brief Destructor for SessionManager. Cleans up the background cleanup thread.
         */
        ~SessionManager() {
            if (cleanupThread_.joinable()) {
                cleanupThread_.request_stop();
                cleanupThread_.join();
            }
        }
        /**
         * @brief Create a new session for a given client identity.
         * @param cid Client identity
         * @param nowMs Current time in milliseconds
         * @return Shared pointer to the created Session
         */
        std::shared_ptr<Session> createSession(const ClientIdentity& cid, uint64_t nowMs);
        /**
         * @brief Get or create a session for a given client identity.
         * @param id Client identity
         * @param nowMs Current time in milliseconds
         * @return Shared pointer to the found or created Session
         */
        std::shared_ptr<Session> getOrCreate(const ClientIdentity& id, uint64_t nowMs);
        /**
         * @brief Remove a session by session ID.
         * @param sid Session ID
         */
        void removeSession(const std::string& sid);

        /**
         * @brief Set a field in a session and update the index.
         * @tparam T Type of the value
         * @param s Shared pointer to the session
         * @param key Field key
         * @param value Value to set
         */
        template<typename T>
        void indexedSet(std::shared_ptr<Session> s,
            const std::string& key,
            const T& value);

        /**
         * @brief Get a session by session ID.
         * @param sid Session ID
         * @return Shared pointer to the Session (nullptr if not found)
         */
        std::shared_ptr<Session> getSession(const std::string& sid) const;
        /**
         * @brief List all active session IDs.
         * @return Vector of session ID strings
         */
        std::vector<std::string> listSessionIds() const;

        /**
         * @brief Attach an existing session to the manager.
         * @param s Shared pointer to the session
         */
        void attachSession(std::shared_ptr<Session> s);

        /**
         * @brief Get the generic index for fast field-based lookup.
         * @return Reference to the GenericIndex
         */
        GenericIndex& indices() { return index_; }     // ★ O(1) findByX

        /**
         * @brief Remove expired sessions based on TTL.
         * @param nowMs Current time in milliseconds
         */
        void reap(uint64_t nowMs);                               // TTL dolanları sil

        /**
         * @brief Start the background cleanup timer thread.
         */
        void startCleanupTimer();

        /**
         * @brief Set a custom field in a session's state.
         * @tparam T Type of the value
         * @param sid Session ID
         * @param key Field key
         * @param value Value to set
         * @param indexed Whether the field should be indexed for fast lookup
         * @return True if the field was set successfully, false otherwise
         */
        template<typename T>
        bool setField(const std::string& sid,
            const std::string& key,
            const T& value,
            bool indexed = false);

        /**
         * @brief Get a custom field value from a session's state.
         * @tparam T Type of the value
         * @param sid Session ID
         * @param key Field key
         * @return Optional containing the value if present, std::nullopt otherwise
         */
        template<typename T>
        std::optional<T> getField(const std::string& sid,
            const std::string& key) const;

        /**
         * @struct OfflineMessage
         * @brief Structure representing an offline message queued for a session.
         */
        struct OfflineMessage {
            std::vector<uint8_t> data;    ///< Message content
            uint64_t timestamp;           ///< Time the message was queued
            std::string sessionId;        ///< Session ID the message belongs to
        };

        /**
         * @brief Add an offline message to a session's queue.
         * @param sessionId Session ID
         * @param data Message data
         * @return True if the message was added, false otherwise
         */
        bool addOfflineMessage(const std::string& sessionId, const std::vector<uint8_t>& data);
    
        /**
         * @brief Process and deliver all offline messages for a session.
         * @param sessionId Session ID
         * @param sendCallback Callback function to send each message
         */
        void processOfflineMessages(const std::string& sessionId, 
                              std::function<void(const std::vector<uint8_t>&)> sendCallback);

    private:
        template<typename T>
        static std::string toStr(const T& v) { return std::to_string(v); }
        static std::string toStr(const std::string& s) { return s; }
        static std::string toStr(const std::vector<std::string>& v) {
            std::string result;
            for (const auto& s : v) {
                if (!result.empty()) result += ",";
                result += s;
            }
            return result;
        }

        //----------------ANA YAPILAR---------------------------//
        std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
        GenericIndex index_;

        // State storage - thread-safe erişim için shared_mutex
        std::unordered_map<std::string, std::unordered_map<std::string, std::any>> state_;
        mutable std::shared_mutex stateMx_;

        mutable std::shared_mutex mx_;        // ← RW-lock

        /* parametreler ----------------------------------------------------- */
        uint64_t            ttlMs_{ 0 };
        std::atomic_uint64_t seq_{ 0 };

        /* kimlik-bazlı hızlı lookup (std::hash<ClientIdentity> hazır) */
        std::unordered_map<ClientIdentity,
            std::shared_ptr<Session>>       byId_;
        /* eski SID map'i – geriye uyumluluk için tutuluyor */
        std::unordered_map<std::string,
            std::shared_ptr<Session>>       bySid_;

        std::jthread cleanupThread_;  // Timer thread'i

        // Her session için offline mesaj kuyruğu
        std::unordered_map<std::string, std::queue<OfflineMessage>> offlineQueues;
        std::mutex queueMutex;

        // Sistem geneli limitler
        static constexpr size_t MAX_QUEUE_SIZE_PER_SESSION = 1000;  // Her session için maksimum mesaj sayısı
        static constexpr size_t MAX_TOTAL_QUEUED_MESSAGES = 100000; // Tüm sistem için maksimum mesaj sayısı
        static constexpr uint64_t MESSAGE_TTL_MS = 24 * 60 * 60 * 1000; // Mesajların yaşam süresi (24 saat)

        size_t totalQueuedMessages = 0;  // Toplam kuyrukta bekleyen mesaj sayısı

        // Eski mesajları temizle
        void cleanupOldMessages();

    };

}
