/**
 * @file session.hpp
 * @brief Session class and connection state for BinaryRPC.
 *
 * Defines the Session class, which holds QoS buffers, user data, and connection state
 * for each client identity (ip + clientId + deviceId) in the BinaryRPC framework.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

#include "binaryrpc/core/auth/ClientIdentity.hpp"
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <chrono>

// If you don't want to include uWebSockets directly, use forward declaration:
namespace uWS { template<bool, bool, typename> class WebSocket; }
struct PerSocketData;

namespace binaryrpc {

struct ConnState;

    /**
     * @enum ConnectionState
     * @brief Connection state for a session.
     *
     * - ONLINE: Client is actively connected
     * - OFFLINE: Client connection is dropped
     */
    enum class ConnectionState {
        ONLINE,     ///< Client is actively connected
        OFFLINE     ///< Client connection is dropped
    };

    /**
     * @class Session
     * @brief Holds QoS buffers, user data, and connection state for a client identity.
     *
     * Manages the lifetime of a client session, including connection state, QoS buffers,
     * key-value storage, and duplicate filtering for reliable message delivery.
     */
    class Session {
    public:
        /**
         * @brief Construct a new Session with a client identity and legacy session ID.
         * @param ident Client identity (clientId, deviceId, sessionToken)
         * @param legacySid Legacy session ID (for backward compatibility)
         */
        Session(ClientIdentity ident, std::string legacySid);
        
        /**
         * @brief Destructor for the Session class.
         */
        ~Session();

        /**
         * @brief Get the legacy session ID.
         * @return Reference to the legacy session ID string
         */
        const std::string& id() const;
        /**
         * @brief Get the client identity.
         * @return Reference to the ClientIdentity
         */
        const ClientIdentity& identity() const;

        /**
         * @brief Rebind the session to a new WebSocket connection.
         *
         * Resets the duplicate filter on new connection.
         * @param ws Pointer to the new WebSocket connection
         */
        using WS = uWS::WebSocket<false, true, PerSocketData>;
        void rebind(WS* ws);
        /**
         * @brief Get the current live WebSocket connection.
         * @return Pointer to the live WebSocket connection (or nullptr if offline)
         */
        WS* liveWs() const;

        /**
         * @brief Shared pointer to QoS state (used by transport layer).
         */
        std::shared_ptr<ConnState> qosState;

        /**
         * @brief Expiry time in milliseconds (0 means not expired).
         */
        uint64_t expiryMs = 0;

        /**
         * @brief Set the legacy connection pointer (deprecated).
         * @param conn Pointer to the legacy connection
         * @deprecated Use rebind() instead.
         */
        [[deprecated("use rebind()")]]
        void setConnection(void* conn);
        /**
         * @brief Get the legacy connection pointer (deprecated).
         * @return Pointer to the legacy connection
         * @deprecated Use liveWs() instead.
         */
        [[deprecated("use liveWs()")]]
        void* getConnection() const;

        /**
         * @brief Set a key-value pair in the session's data store.
         * @tparam T Type of the value
         * @param key Key string
         * @param value Value to set
         */
        template<typename T>
        void set(const std::string& key, T value);
        /**
         * @brief Get a value from the session's data store by key.
         * @tparam T Type of the value
         * @param key Key string
         * @return Value of type T if present, default-constructed T otherwise
         */
        template<typename T>
        T get(const std::string& key) const;

        /**
         * @brief Check for duplicate QoS-1 messages using the duplicate filter.
         * @param rpcPayload Payload of the RPC message
         * @param ttl Time-to-live for duplicate detection
         * @return True if the message is not a duplicate, false otherwise
         */
        bool acceptDuplicate(const std::vector<uint8_t>& rpcPayload, std::chrono::milliseconds ttl);

        /**
         * @brief Current connection state (ONLINE or OFFLINE).
         */
        ConnectionState connectionState = ConnectionState::OFFLINE;
    private:
        // PIMPL idiom to hide internal implementation details
        struct Impl;
        std::unique_ptr<Impl> pImpl_;
    };

}
