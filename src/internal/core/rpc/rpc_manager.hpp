/**
 * @file rpc_manager.hpp
 * @brief RPCManager class for registering and dispatching RPC methods in BinaryRPC.
 *
 * Provides registration and invocation of RPC handlers, supporting both context-based
 * and legacy request/response handler styles. Thread-safe for concurrent use.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/rpc/rpc_context.hpp"
#include "binaryrpc/core/interfaces/itransport.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "binaryrpc/core/types.hpp"

namespace binaryrpc {

    /**
     * @typedef InternalHandler
     * @brief Internal handler type: used within the framework for request/response style.
     *
     * Receives request data, a response buffer, and a Session reference.
     */
    using InternalHandler =
        std::function<void(const std::vector<uint8_t>&,
            std::vector<uint8_t>&,
            Session&)>;

    /**
     * @class RPCManager
     * @brief Manages registration and invocation of RPC methods in BinaryRPC.
     *
     * Supports both context-based and legacy request/response handler styles.
     * Thread-safe for concurrent registration and invocation.
     */
    class RPCManager {
    public:
        /**
         * @brief Register an RPC handler using the context-based interface.
         *
         * The handler receives the request data and an RpcContext object.
         * @param method Name of the RPC method
         * @param handler Handler function to register
         * @param transport Pointer to the transport (used for replies)
         */
        void registerRPC(const std::string& method,
            RpcContextHandler handler,
            ITransport* transport);

        /**
         * @brief Register an RPC handler using the legacy request/response style.
         *
         * The handler receives the request data, a response buffer, and a Session reference.
         * @param method Name of the RPC method
         * @param handler Handler function to register
         */
        void registerRPC(const std::string& method,
            InternalHandler handler);

        /**
         * @brief Invoke a registered RPC handler by method name.
         *
         * Looks up the handler and calls it with the provided request, response, and session.
         * @param method Name of the RPC method
         * @param request Request data
         * @param response Buffer to store the response data
         * @param session Reference to the session
         * @return True if the handler was found and called, false otherwise
         */
        bool call(const std::string& method,
            const std::vector<uint8_t>& request,
            std::vector<uint8_t>& response,
            Session& session);

    private:
        std::unordered_map<std::string, InternalHandler> handlers_; ///< Registered RPC handlers
        mutable std::mutex mutex_;  ///< Protects access to the handlers map
    };

}