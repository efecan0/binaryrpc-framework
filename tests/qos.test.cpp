#include <catch2/catch_all.hpp>
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/strategies/linear_backoff.hpp"
#include "binaryrpc/core/strategies/exponential_backoff.hpp"
#include <thread>
#include <binaryrpc/core/util/byteorder.hpp>

using namespace binaryrpc;

TEST_CASE("makeFrame constructs correct binary layout", "[unit]") {
    uint64_t id = 12345;
    std::vector<uint8_t> payload = {1, 2, 3, 4};
    auto frame = WebSocketTransport::test_makeFrame(FrameType::FRAME_DATA, id, payload);

    REQUIRE(frame.size() == 1 + sizeof(id) + payload.size());
    REQUIRE(static_cast<FrameType>(frame[0]) == FrameType::FRAME_DATA);
    uint64_t extractedId;
    std::memcpy(&extractedId, frame.data() + 1, sizeof(id));
    extractedId = networkToHost64(extractedId);
    REQUIRE(extractedId == id);

    std::vector<uint8_t> extractedPayload(frame.begin() + 1 + sizeof(id), frame.end());
    REQUIRE(extractedPayload == payload);
}

TEST_CASE("registerSeen prevents duplicate within TTL", "[unit]") {
    ConnState state;
    uint64_t id = 42;
    uint32_t ttlMs = 1000;

    bool firstInsert = WebSocketTransport::test_registerSeen(&state, id, ttlMs);
    REQUIRE(firstInsert == true);

    bool secondInsert  = WebSocketTransport::test_registerSeen(&state, id, ttlMs);
    REQUIRE(secondInsert == false);

    // Simulate TTL expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(ttlMs + 10));

    bool thirdInsert   = WebSocketTransport::test_registerSeen(&state, id, ttlMs);
    REQUIRE(thirdInsert == true);
}

TEST_CASE("LinearBackoff increases linearly up to max", "[unit]") {
    auto backoff = LinearBackoff(std::chrono::milliseconds(10), std::chrono::milliseconds(50));

    REQUIRE(backoff.nextDelay(1) == std::chrono::milliseconds(10));
    REQUIRE(backoff.nextDelay(2) == std::chrono::milliseconds(20));
    REQUIRE(backoff.nextDelay(3) == std::chrono::milliseconds(30));
    REQUIRE(backoff.nextDelay(4) == std::chrono::milliseconds(40));
    REQUIRE(backoff.nextDelay(5) == std::chrono::milliseconds(50));
    REQUIRE(backoff.nextDelay(6) == std::chrono::milliseconds(50));

}

TEST_CASE("ExponentialBackoff doubles until max", "[unit]") {
    auto backoff = ExponentialBackoff(std::chrono::milliseconds(10), std::chrono::milliseconds(80));
    REQUIRE(backoff.nextDelay(1) == std::chrono::milliseconds(10));
    REQUIRE(backoff.nextDelay(2) == std::chrono::milliseconds(20));
    REQUIRE(backoff.nextDelay(3) == std::chrono::milliseconds(40));
    REQUIRE(backoff.nextDelay(4) == std::chrono::milliseconds(80));
    REQUIRE(backoff.nextDelay(5) == std::chrono::milliseconds(80));

}
