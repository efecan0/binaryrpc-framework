/**
 * @file middleware_chain.hpp
 * @brief MiddlewareChain for composing and executing middleware in BinaryRPC.
 *
 * Provides a chain-of-responsibility pattern for middleware, allowing global and method-specific
 * middleware to be registered and executed in order for each RPC call.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <vector>
#include <unordered_map>
#include "binaryrpc/core/session/session.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "binaryrpc/core/types.hpp"

namespace binaryrpc {

    /**
     * @class MiddlewareChain
     * @brief Composes and executes middleware for RPC calls in BinaryRPC.
     *
     * Supports both global and method-specific middleware. Executes middleware in order,
     * allowing each to call next() to proceed or halt the chain.
     */
    class MiddlewareChain {
    public:
        /**
         * @brief Add a global middleware to the chain.
         * @param mw Middleware function to add
         */
        void add(Middleware mw);

        /**
         * @brief Add a middleware for a specific method.
         * @param method Method name
         * @param mw Middleware function to add
         */
        void addFor(const std::string& method, Middleware mw);

        /**
         * @brief Execute the middleware chain for a session, method, and payload.
         *
         * Calls each middleware in order. If all call next(), returns true. If any middleware
         * does not call next(), the chain is halted and returns false.
         *
         * @param s Reference to the session
         * @param method Method name
         * @param payload Payload data (may be modified by middleware)
         * @return True if the entire chain was executed, false if halted
         */
        bool execute(Session& s, const std::string& method, std::vector<uint8_t>& payload);

    private:
        std::vector<Middleware> global_; ///< Global middleware chain
        std::unordered_map<std::string, std::vector<Middleware>> scoped_; ///< Method-specific middleware
    };

}