/**
 * @file iprotocol.hpp
 * @brief Interface for serialization protocols in BinaryRPC.
 *
 * Defines the IProtocol interface and ParsedRequest struct for implementing custom
 * serialization and deserialization protocols for RPC communication.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <vector>
#include <string>
#include "binaryrpc/core/util/error_types.hpp"

namespace binaryrpc {

    /**
     * @struct ParsedRequest
     * @brief Structure representing a parsed RPC request.
     */
    struct ParsedRequest {
        std::string methodName; ///< Name of the RPC method
        std::vector<uint8_t> payload; ///< Payload data
    };

    /**
     * @class IProtocol
     * @brief Interface for custom serialization protocols in BinaryRPC.
     *
     * Implement this interface to provide custom serialization and deserialization
     * for RPC requests, responses, and errors.
     */
    class IProtocol {
    public:
        virtual ~IProtocol() = default;
        /**
         * @brief Parse input data into a ParsedRequest structure.
         * @param data Input data as a byte vector
         * @return ParsedRequest containing the method name and payload
         */
        virtual ParsedRequest parse(const std::vector<uint8_t>& data) = 0;
        /**
         * @brief Serialize a method and payload into a byte vector.
         * @param method Method name
         * @param payload Payload data
         * @return Serialized data as a byte vector
         */
        virtual std::vector<uint8_t> serialize(const std::string& method, const std::vector<uint8_t>& payload) = 0;
        /**
         * @brief Serialize an error object into a byte vector.
         * @param e Error object
         * @return Serialized error data as a byte vector
         */
        virtual std::vector<uint8_t> serializeError(const ErrorObj&) = 0;
    };

}
