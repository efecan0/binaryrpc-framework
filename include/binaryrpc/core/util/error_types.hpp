/**
 * @file error_types.hpp
 * @brief Error type definitions for BinaryRPC.
 *
 * Provides enums and structures for representing error codes and error objects
 * used in RPC error handling and propagation.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <cstdint>

namespace binaryrpc {

    /**
     * @enum RpcErr
     * @brief Error codes for RPC operations.
     *
     * - Parse: Parsing error
     * - Middleware: Middleware denied the request
     * - NotFound: RPC method not found
     * - Auth: Authentication error
     * - RateLimited: Rate limiting triggered
     * - Internal: Internal server error
     */
    enum class RpcErr : int {
        Parse = 1,       ///< Parsing error
        Middleware,      ///< Middleware denied the request
        NotFound,        ///< RPC method not found
        Auth,            ///< Authentication error
        RateLimited,     ///< Rate limiting triggered
        Internal = 99    ///< Internal server error
    };

    /**
     * @struct ErrorObj
     * @brief Structure representing an error object for RPC error propagation.
     */
    struct ErrorObj {
        RpcErr                       code; ///< Error code
        std::string                  msg;  ///< Error message
        std::vector<uint8_t>         data; ///< Optional error data
    };

}
