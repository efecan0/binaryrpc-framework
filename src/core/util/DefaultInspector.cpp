#include "binaryrpc/core/util/DefaultInspector.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <uwebsockets/App.h>
#include <string>

namespace binaryrpc {

    struct DefaultInspector::Impl {
        // Implementation details can be added here if needed in the future.
    };

    DefaultInspector::DefaultInspector() : pImpl_(std::make_unique<Impl>()) {}
    
    DefaultInspector::~DefaultInspector() = default;
    
    DefaultInspector::DefaultInspector(DefaultInspector&&) noexcept = default;
    
    DefaultInspector& DefaultInspector::operator=(DefaultInspector&&) noexcept = default;

    std::optional<ClientIdentity> DefaultInspector::extract(uWS::HttpRequest& req) {
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

    std::string DefaultInspector::rejectReason() const {
        return "Invalid handshake data";
    }

} // namespace binaryrpc 