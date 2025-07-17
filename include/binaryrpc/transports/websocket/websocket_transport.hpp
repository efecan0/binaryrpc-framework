/**
 * @file websocket_transport.hpp
 * @brief WebSocket transport layer for BinaryRPC.
 */
#pragma once

#include "binaryrpc/core/interfaces/itransport.hpp"
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>

namespace binaryrpc {
    // Forward declarations for opaque types used in the public API
    class SessionManager;
    class IHandshakeInspector;
    struct ReliableOptions;
    class Session;
    enum FrameType : uint8_t;

    /**
     * @class WebSocketTransport
     * @brief ITransport implementation using uWebSockets.
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
        WebSocketTransport(SessionManager& sm, uint16_t idleTimeoutSec = 45, uint32_t maxPayloadBytes = 10 * 1024 * 1024);

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
        void setReliable(const ReliableOptions& options) override;

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



