/**
 * @file DefaultInspector.hpp
 * @brief Default handshake inspector for BinaryRPC WebSocket connections.
 *
 * Provides a basic implementation of IHandshakeInspector that extracts client identity
 * from HTTP headers and always authorizes the connection by default.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once

#include "binaryrpc/core/interfaces/IHandshakeInspector.hpp"
#include "binaryrpc/core/auth/ClientIdentity.hpp"
#include <optional>
#include <string>
#include <memory>

// Forward declaration to avoid including the uWebSockets header
namespace uWS { struct HttpRequest; }

namespace binaryrpc {

    /**
     * @class DefaultInspector
     * @brief Default implementation of IHandshakeInspector for extracting client identity.
     *
     * Extracts clientId, deviceId, and sessionToken from HTTP headers:
     *   - clientId: "x-client-id" (required)
     *   - deviceId: "x-device-id" (optional, parsed as uint64_t)
     *   - sessionToken: "x-session-token" (optional, 32-character hex)
     *
     * Always authorizes the connection if clientId is present and deviceId is valid.
     */
    class DefaultInspector : public IHandshakeInspector {
    public:
        DefaultInspector();
        ~DefaultInspector() override;

        DefaultInspector(const DefaultInspector&) = delete;
        DefaultInspector& operator=(const DefaultInspector&) = delete;
        DefaultInspector(DefaultInspector&&) noexcept;
        DefaultInspector& operator=(DefaultInspector&&) noexcept;

        /**
         * @brief Extract client identity from the HTTP upgrade request and remote IP.
         * @param req Reference to the uWS::HttpRequest
         * @return Optional ClientIdentity if extraction is successful, std::nullopt otherwise
         */
        std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override;

        /**
         * @brief Reason for handshake rejection (not used in default implementation).
         * @return String describing the rejection reason
         */
        std::string rejectReason() const override;

    private:
        // PIMPL idiom to hide implementation details
        struct Impl;
        std::unique_ptr<Impl> pImpl_;
    };

}