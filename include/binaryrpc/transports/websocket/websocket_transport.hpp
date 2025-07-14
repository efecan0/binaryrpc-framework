/**
 * @file websocket_transport.hpp
 * @brief WebSocket transport layer for BinaryRPC, providing session management, QoS, and message delivery over WebSocket.
 *
 * This transport implements the ITransport interface using uWebSockets as the underlying protocol.
 * It supports advanced features such as session management, Quality of Service (QoS) levels, offline message queuing,
 * and pluggable handshake inspection. Designed for high-performance, scalable, and reliable RPC communication.
 */
#pragma once

#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include "binaryrpc/core/util/qos.hpp"
#include <memory>
#include <cstdint>
#include <deque>
#include <string_view>
#include "binaryrpc/core/util/conn_state.hpp"
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/interfaces/IHandshakeInspector.hpp"
#include "binaryrpc/core/util/hex.hpp"
#include "binaryrpc/core/util/random.hpp"
#include "binaryrpc/core/util/time.hpp"
#include <uwebsockets/App.h>


#include <unordered_map>
#include <unordered_set>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <chrono>
#include <iostream>

#ifdef BINARYRPC_TEST
    #include <functional>
    #include <vector>
#endif

namespace binaryrpc {

/**
 * @enum FrameType
 * @brief Types of frames exchanged over WebSocket for QoS and control.
 *
 * These frame types are used to implement different Quality of Service (QoS) levels and message delivery guarantees.
 */
    enum FrameType : uint8_t {
    FRAME_DATA      = 0x00,  /**< QoS1 data frame (AtLeastOnce delivery) */
    FRAME_ACK       = 0x01,  /**< QoS1 acknowledgment frame */
    FRAME_PREPARE   = 0x02,  /**< QoS2 prepare (PUBLISH) frame (ExactlyOnce delivery) */
    FRAME_PREPARE_ACK = 0x03,/**< QoS2 prepare acknowledgment (PUBREC) frame */
    FRAME_COMMIT    = 0x04,  /**< QoS2 commit (PUBREL) frame */
    FRAME_COMPLETE  = 0x05   /**< QoS2 complete (PUBCOMP) frame */
    };

/**
 * @struct PerSocketData
 * @brief Per-connection state for each WebSocket client.
 *
 * Stores session, connection state, activity timestamps, and outgoing message queue for each WebSocket connection.
 * Used internally by the WebSocket transport to manage client-specific data and message delivery.
 */
    struct PerSocketData {
    /**
     * @brief Shared pointer to the session associated with this connection.
     */
        std::shared_ptr<Session>   session;
    /**
     * @brief Shared pointer to the connection state (QoS, pending messages, etc.).
     */
        std::shared_ptr<ConnState> state{std::make_shared<ConnState>()};
    /**
     * @brief Last activity timestamp for idle timeout management.
     */
        std::chrono::steady_clock::time_point lastActive{};
    /**
     * @brief Indicates if the connection is alive.
     */
    std::atomic_bool          alive{true};
    /**
     * @brief Pointer to the uWebSockets event loop for this connection.
     */
        uWS::Loop*                 loop{nullptr};
    /**
     * @brief Outgoing message queue for this connection.
     */
    std::deque<std::vector<uint8_t>> sendQueue;

    /**
     * @brief Default constructor.
     */
        PerSocketData() = default;

    /**
     * @brief Copy constructor.
     * @param o The PerSocketData to copy from.
     */
        PerSocketData(const PerSocketData& o)
        : session(o.session)
        , state(o.state)
            , lastActive(o.lastActive)
        , alive(true)
        , loop(nullptr)
        , sendQueue()
    {}

    /**
     * @brief Copy assignment operator.
     * @param o The PerSocketData to copy from.
     * @return Reference to this object.
     */
        PerSocketData& operator=(const PerSocketData& o) {
            if (this != &o) {
                session    = o.session;
                state      = o.state;
                lastActive = o.lastActive;
            alive.store(true, std::memory_order_relaxed);
                loop       = nullptr;
            sendQueue.clear();
            }
            return *this;
        }

    /**
     * @brief Move constructor.
     * @param other The PerSocketData to move from.
     */
        PerSocketData(PerSocketData&& other) noexcept
            : session(std::move(other.session))
            , state(std::move(other.state))
            , lastActive(other.lastActive)
            , alive(other.alive.load(std::memory_order_relaxed))
            , loop(other.loop)
            , sendQueue(std::move(other.sendQueue))
        {
            other.loop = nullptr;
        }

    /**
     * @brief Move assignment operator.
     * @param other The PerSocketData to move from.
     * @return Reference to this object.
     */
        PerSocketData& operator=(PerSocketData&& other) noexcept {
            if (this != &other) {
                session = std::move(other.session);
                state = std::move(other.state);
                lastActive = other.lastActive;
                alive.store(other.alive.load(std::memory_order_relaxed), std::memory_order_relaxed);
                loop = other.loop;
                sendQueue = std::move(other.sendQueue);
                other.loop = nullptr;
            }
            return *this;
        }
    };

/**
 * @class WebSocketTransport
 * @brief Implements the ITransport interface using WebSocket as the underlying protocol.
 *
 * Handles session management, Quality of Service (QoS), message delivery, and connection lifecycle for BinaryRPC over WebSocket.
 * Supports pluggable handshake inspection, offline message queuing, and advanced reliability options.
 */
    class WebSocketTransport : public ITransport {
    public:
    /**
     * @brief Constructs a WebSocketTransport instance.
     * @param sm Reference to the SessionManager.
     * @param idleTimeoutSec Idle timeout in seconds (default: 45).
     * @param maxPayloadBytes Maximum payload size in bytes (default: 10 MB).
     */
        WebSocketTransport(SessionManager& sm, uint16_t idleTimeoutSec = 45, uint32_t     maxPayloadBytes = 10 * 1024 * 1024);

    /**
     * @brief Destructor.
     */
        ~WebSocketTransport() override;

    /**
     * @brief Starts the WebSocket server and begins listening on the specified port.
     * @param port The port number to listen on.
     */
        void start(uint16_t port) override;

    /**
     * @brief Stops the WebSocket server and releases all resources.
     */
        void stop() override;

    /**
     * @brief Sends data to all connected clients.
     * @param data The data to send.
     */
        void send(const std::vector<uint8_t>& data) override;

    /**
     * @brief Sends data to a specific client connection.
     * @param connection The client connection pointer.
     * @param data The data to send.
     */
        void sendToClient(void* connection, const std::vector<uint8_t>& data) override;

    /**
     * @brief Sends data to a specific session.
     * @param session The target session.
     * @param data The data to send.
     */
        void sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data) override;
        
    /**
     * @brief Disconnects a specific client connection.
     * @param connection The client connection pointer.
     */
        void disconnectClient(void* connection) override;

    /**
     * @brief Configures reliability and QoS options for message delivery.
     * @param options The reliability options to use.
     */
        void setReliable(const ReliableOptions&) override;

    /**
     * @brief Sets the callback to be invoked when data is received from a client.
     * @param cb The data callback function.
     */
    void setCallback(DataCallback cb) override;

    /**
     * @brief Sets the callback to be invoked when a session is registered.
     * @param cb The session registration callback function.
     */
    void setSessionRegisterCallback(SessionRegisterCallback cb) override;

    /**
     * @brief Sets the callback to be invoked when a client disconnects.
     * @param cb The disconnect callback function.
     */
    void setDisconnectCallback(DisconnectCallback cb) override;

    /**
     * @brief Sets the handshake inspector for custom authentication or validation during WebSocket upgrade.
     * @param inspector The handshake inspector instance.
     */
    void setHandshakeInspector(std::shared_ptr<IHandshakeInspector> inspector);

#ifdef BINARYRPC_TEST
    /**
     * @brief Simulates receiving a raw frame from a client (for testing purposes).
     * @param frame The raw frame data.
     */
        void onRawFrameFromClient(const std::vector<uint8_t>& frame);

    /**
     * @brief Sets a send interceptor callback for testing outgoing frames.
     * @param interceptor The interceptor function.
     */
    void setSendInterceptor(std::function<void(const std::vector<uint8_t>&)> interceptor);

    /**
     * @brief Creates a test frame for unit testing.
     * @param type The frame type.
     * @param id The message ID.
     * @param payload The payload data.
     * @return The constructed frame as a byte vector.
     */
    static std::vector<uint8_t> test_makeFrame(FrameType type, uint64_t id, const std::vector<uint8_t>& payload);

    /**
     * @brief Registers a message ID as seen for duplicate detection (for testing).
     * @param connStatePtr Pointer to the connection state.
     * @param id The message ID.
     * @param ttlMs Time-to-live in milliseconds.
     * @return True if the ID was newly registered, false if it was already seen.
     */
    static bool test_registerSeen(void* connStatePtr, uint64_t id, uint32_t ttlMs);
#endif
    private:
    /**
     * @brief Private implementation (PIMPL) to hide internal details and reduce compile-time dependencies.
     */
        class Impl;
        std::unique_ptr<Impl> pImpl_;
        std::shared_ptr<IHandshakeInspector> getInspector();
        #ifdef BINARYRPC_TEST
            void handleFrame(const uint8_t* data, std::size_t length);
            std::function<void(const std::vector<uint8_t>&)> sendInterceptor_;
        #endif
    };

}



