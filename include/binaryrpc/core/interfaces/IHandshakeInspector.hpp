/**
 * @file IHandshakeInspector.hpp
 * @brief Interface for custom WebSocket handshake inspection and authorization in BinaryRPC.
 *
 * Defines the IHandshakeInspector interface for extracting and authorizing client identity
 * during the WebSocket handshake process.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include "binaryrpc/core/auth/ClientIdentity.hpp"

namespace uWS {
    class HttpRequest;
}

namespace binaryrpc {

    /**
     * @class IHandshakeInspector
     * @brief Interface for extracting and authorizing client identity during WebSocket handshake.
     *
     * Implement this interface to perform custom header/IP extraction and authorization logic
     * for incoming WebSocket upgrade requests.
     */
    class IHandshakeInspector {
    public:
        virtual ~IHandshakeInspector() = default;

        /**
         * @brief Extract a ClientIdentity from the handshake request.
         *
         * Implementations should parse headers or other request data to build a ClientIdentity.
         * Returning std::nullopt will reject the handshake.
         *
         * @param req WebSocket HTTP upgrade request.
         * @return std::optional<ClientIdentity> if extraction (and basic validation) succeeds; std::nullopt to reject.
         */
        virtual std::optional<ClientIdentity>
            extract(uWS::HttpRequest& req) = 0;

        /**
         * @brief Optionally perform authorization based on extracted identity and full request.
         *
         * Default implementation always returns true (accepts all connections).
         * Override to implement custom authorization logic.
         *
         * @param identity Extracted client identity
         * @param req WebSocket HTTP upgrade request
         * @return true to authorize, false to reject
         */
        virtual bool authorize(const ClientIdentity& identity, const uWS::HttpRequest& req) {
            return true;
        }

        /**
         * @brief Reason text sent when rejecting a handshake.
         *
         * Default is "unauthorized". Override to provide a custom rejection reason.
         *
         * @return String describing the rejection reason
         */
        virtual std::string rejectReason() const {
            return "unauthorized";
        }
    };

}