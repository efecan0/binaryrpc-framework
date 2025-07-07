/**
 * @file itransport.hpp
 * @brief Interface for transport layers in BinaryRPC.
 *
 * Defines the ITransport interface for implementing custom transport layers
 * (e.g., WebSocket, TCP) for BinaryRPC communication.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <functional>
#include <vector>
#include <memory>
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/util/qos.hpp"

namespace binaryrpc {

    /**
     * @typedef DataCallback
     * @brief Callback type for receiving data from the transport.
     */
    using DataCallback = std::function<void(const std::vector<uint8_t>&, std::shared_ptr<Session>, void*)>;
    /**
     * @typedef SessionRegisterCallback
     * @brief Callback type for registering a new session.
     */
    using SessionRegisterCallback = std::function<void(const std::string&, std::shared_ptr<Session>)>;
    /**
     * @typedef DisconnectCallback
     * @brief Callback type for handling client disconnections.
     */
    using DisconnectCallback = std::function<void(std::shared_ptr<Session>)>;

    /**
     * @class ITransport
     * @brief Interface for custom transport layers in BinaryRPC.
     *
     * Implement this interface to provide custom transport mechanisms (e.g., WebSocket, TCP).
     */
    class ITransport {
    public:
        virtual ~ITransport() = default;
        /**
         * @brief Start the transport on the specified port.
         * @param port Port number to listen on
         */
        virtual void start(uint16_t port) = 0;
        /**
         * @brief Stop the transport.
         */
        virtual void stop() = 0;
        /**
         * @brief Send data to all clients.
         * @param data Data to send
         */
        virtual void send(const std::vector<uint8_t>& data) = 0;
        /**
         * @brief Send data to a specific client connection.
         * @param connection Pointer to the client connection
         * @param data Data to send
         */
        virtual void sendToClient(void* connection, const std::vector<uint8_t>& data) = 0;
        /**
         * @brief Send data to a specific session.
         * @param session Shared pointer to the session
         * @param data Data to send
         */
        virtual void sendToSession(std::shared_ptr<Session> session, const std::vector<uint8_t>& data) = 0;
        /**
         * @brief Disconnect a specific client connection.
         * @param connection Pointer to the client connection
         */
        virtual void disconnectClient(void* connection) = 0;
        /**
         * @brief Set the callback for receiving data from clients.
         * @param callback DataCallback function
         */
        virtual void setCallback(DataCallback callback) = 0;
        /**
         * @brief Set the callback for session registration.
         * @param cb SessionRegisterCallback function
         */
        virtual void setSessionRegisterCallback(SessionRegisterCallback cb) = 0;
        /**
         * @brief Set the callback for client disconnections.
         * @param cb DisconnectCallback function
         */
        virtual void setDisconnectCallback(DisconnectCallback cb) = 0;
        /**
         * @brief Set reliable delivery options for the transport.
         * @param options ReliableOptions struct
         */
        virtual void setReliable(const ReliableOptions&) = 0;
    };
}
