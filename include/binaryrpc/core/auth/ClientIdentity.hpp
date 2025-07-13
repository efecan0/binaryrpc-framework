#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace binaryrpc {


    /**
     * @brief Transport-agnostic client identity.
     *
     *    ┌──────────┬───────────┬──────────────────────────────┐
     *    │ clientId │ deviceId  │ sessionToken (128-bit rand.) │
     *    └──────────┴───────────┴──────────────────────────────┘
     *
     * • IP address is **not** used in equality or hashing (may change on mobile networks).
     * • sessionToken is only used for reconnect/authentication purposes.
     */
    struct ClientIdentity {
        std::string                     clientId;
        std::uint64_t                   deviceId{};
        std::array<std::uint8_t, 16>    sessionToken{};   ///< Random UUID compliant with RFC 4122

        /* Identity equality — ignores IP and token */
        [[nodiscard]] bool operator==(const ClientIdentity& o) const noexcept {
            return clientId == o.clientId && deviceId == o.deviceId;
        }
    };

    /* ---------- utility  ---------- */
    inline void hashCombine(std::size_t& seed, std::size_t v) noexcept {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

}

/* ---------- std::hash specialization ---------- */
template<>
struct std::hash<binaryrpc::ClientIdentity> {
    std::size_t operator()(const binaryrpc::ClientIdentity& id) const noexcept {
        std::size_t h = std::hash<std::string>{}(id.clientId);
        binaryrpc::hashCombine(h, std::hash<std::uint64_t>{}(id.deviceId));
        return h;                 // sessionToken is not included in hash → stable across reconnects
    }
};