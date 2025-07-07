/**
 * @file msgpack_protocol.hpp
 * @brief MsgPackProtocol for MessagePack-based RPC serialization in BinaryRPC.
 *
 * Implements the IProtocol interface using MessagePack for efficient binary serialization
 * and deserialization of RPC requests, responses, and errors.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include <string>
#include <vector>
#include <msgpack.hpp>
#include <string>
#include <stdexcept>   // for std::overflow_error
#include <limits>
#include <type_traits>

namespace binaryrpc {

    /**
     * @class MsgPackProtocol
     * @brief MessagePack-based protocol implementation for BinaryRPC.
     *
     * Serializes and parses RPC requests, responses, and errors using the MessagePack format.
     */
    class MsgPackProtocol : public IProtocol {
    public:
        /**
         * @brief Parse a MessagePack-encoded RPC request.
         *
         * Extracts the method name and payload from the MessagePack map.
         * @param data MessagePack-encoded input data
         * @return ParsedRequest containing the method name and payload
         */
        ParsedRequest parse(const std::vector<uint8_t>& data) override;

        /**
         * @brief Serialize an RPC method and payload into MessagePack format.
         *
         * Packs the method name and payload as a MessagePack map.
         * @param method Method name
         * @param payload Payload data
         * @return Serialized MessagePack data
         * @throws std::overflow_error if payload size exceeds 4 GiB
         */
        std::vector<uint8_t> serialize(const std::string& method, const std::vector<uint8_t>& payload) override;

        /**
         * @brief Serialize an error object into MessagePack format.
         *
         * Packs the error code, message, and optional data as a MessagePack map.
         * @param e Error object
         * @return Serialized MessagePack error data
         * @throws std::overflow_error if error data size exceeds 4 GiB
         */
        std::vector<uint8_t> serializeError(const ErrorObj& e) override;

    };

}