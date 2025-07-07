/**
 * @file rpc_context.hpp
 * @brief RpcContext class for handling RPC method calls in BinaryRPC.
 *
 * Provides access to the session, transport, and connection for an RPC call,
 * and helper methods for replying, broadcasting, and disconnecting.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"

namespace binaryrpc {
    /**
     * @class RpcContext
     * @brief Context for an RPC method call, providing access to session, transport, and helpers.
     *
     * Used by RPC handlers to reply to the client, broadcast messages, disconnect, and access session data.
     */
    class RpcContext {
    public:
        /**
         * @brief Construct an RpcContext for a session, connection, and transport.
         * @param session Shared pointer to the session
         * @param connection Pointer to the client connection
         * @param transport Pointer to the transport
         */
        RpcContext(std::shared_ptr<Session> session, void* connection, ITransport* transport);

        /**
         * @brief Send a reply to the client for this RPC call.
         * @param data Data to send as the reply
         */
        void reply(const std::vector<uint8_t>& data) const;

        /**
         * @brief Broadcast a message to all clients.
         * @param data Data to broadcast
         */
        void broadcast(const std::vector<uint8_t>& data) const;

        /**
         * @brief Disconnect the client associated with this RPC call.
         */
        void disconnect() const;

        /**
         * @brief Get a reference to the session.
         * @return Reference to the Session
         */
        Session& session();
        /**
         * @brief Get a const reference to the session.
         * @return Const reference to the Session
         */
        const Session& session() const;
        /**
         * @brief Get a shared pointer to the session.
         * @return Shared pointer to the Session
         */
        std::shared_ptr<Session> sessionPtr() const;

        /**
         * @brief Check if the session has the specified role.
         * @param expected Expected role string
         * @return True if the session's "role" field matches, false otherwise
         */
        bool hasRole(const std::string& expected) const;

    private:
        std::shared_ptr<Session> session_; ///< Session associated with the RPC call
        void* connection_;                 ///< Pointer to the client connection
        ITransport* transport_;            ///< Pointer to the transport
    };
}