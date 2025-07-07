/**
 * @file simple_text_protocol.hpp
 * @brief SimpleTextProtocol for text-based RPC serialization in BinaryRPC.
 *
 * Implements the IProtocol interface using a simple colon-separated text format
 * for easy debugging and human readability.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include "binaryrpc/core/util/logger.hpp"

namespace binaryrpc {

    /**
     * @class SimpleTextProtocol
     * @brief Simple text-based protocol implementation for BinaryRPC.
     *
     * Serializes and parses RPC requests, responses, and errors using a colon-separated text format.
     */
    class SimpleTextProtocol : public IProtocol {
    public:
        /**
         * @brief Parse a colon-separated text RPC request.
         *
         * Extracts the method name and payload from the input string.
         * @param data Input data as a byte vector
         * @return ParsedRequest containing the method name and payload
         */
        ParsedRequest parse(const std::vector<uint8_t>& data) override;

        /**
         * @brief Serialize an RPC method and payload into colon-separated text format.
         *
         * Concatenates the method name and payload with a colon separator.
         * @param method Method name
         * @param payload Payload data
         * @return Serialized text data as a byte vector
         */
        std::vector<uint8_t> serialize(const std::string& method, const std::vector<uint8_t>& payload) override;

        /**
         * @brief Serialize an error object into colon-separated text format.
         *
         * Formats the error code and message as a colon-separated string.
         * @param e Error object
         * @return Serialized error data as a byte vector
         */
        std::vector<uint8_t> serializeError(const ErrorObj& e) override;
    };
        
}
