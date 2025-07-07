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
#include <string_view>

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
        /**
         * @brief Extract client identity from the HTTP upgrade request and remote IP.
         * @param req Reference to the uWS::HttpRequest
         * @return Optional ClientIdentity if extraction is successful, std::nullopt otherwise
         */
        std::optional<ClientIdentity> extract(uWS::HttpRequest& req) override {
            using std::string;
            string cid{ req.getHeader("x-client-id") };
            if (cid.empty()) {
                LOG_ERROR("Missing x-client-id header");
                return std::nullopt;
            }

            /* deviceId --> uint64 */
            std::uint64_t did = 0;
            string didTxt{ req.getHeader("x-device-id") };
            if (!didTxt.empty()) {
                // Accept formats like "777" or "dev-777" – parse numeric suffix
                const auto firstDigit = didTxt.find_first_of("0123456789");
                if (firstDigit != std::string::npos) {
                    try {
                        did = std::stoull(didTxt.substr(firstDigit));
                    }
                    catch (const std::exception& ex) {
                        LOG_ERROR("Invalid device id '" + didTxt + "' – " + ex.what());
                        return std::nullopt;
                    }
                }
                else {
                    LOG_ERROR("Device id '" + didTxt + "' contains no numeric part");
                    return std::nullopt;
                }
            }

            /* optional 128-bit token (hex) */
            std::array<std::uint8_t, 16> tok{};
            string tokTxt{ req.getHeader("x-session-token") };
            if (!tokTxt.empty() && tokTxt.size() == 32) {
                try {
                    for (size_t i = 0; i < 16; ++i)
                        tok[i] = static_cast<std::uint8_t>(
                            std::stoi(tokTxt.substr(i * 2, 2), nullptr, 16));
                }
                catch (const std::exception& ex) {
                    LOG_ERROR("Invalid session token format: " + std::string(ex.what()));
                    return std::nullopt;
                }
            }

            return ClientIdentity{ cid, did, tok };
        }

        /**
         * @brief Reason for handshake rejection (not used in default implementation).
         * @return String describing the rejection reason
         */
        std::string rejectReason() const override {
            return "Invalid handshake data";
        }
    };

}