#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <cstring>  // memcpy iÃ§in
#include <cstdio>   // printf iÃ§in
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"

namespace binaryrpc {

class WebSocketTransport;

class MockClient {
public:
    enum class AckMode { None, DropFirst, DropAll };  // âœ… yeni enum

    // ctor: transport'a kendimizi baÄŸlÄ±yoruz
    MockClient(WebSocketTransport& transport)
        : transport_(transport),
        mode_(AckMode::None),
        firstAckDropped_(false)
    {
        // Sunucu â†’ client yÃ¶nlÃ¼ â€œframeâ€ Ã§Ä±ktÄ±sÄ±nÄ± yakala
        transport_.setSendInterceptor([this](auto const& data) {
            if (mode_ == AckMode::DropFirst && !firstAckDropped_) {
                firstAckDropped_ = true;
                printf("[MOCK] dropped first ACK intentionally.\n");
                return;
            }
            this->onServerFrame(data);
        });

    }

    // ACK davranÄ±ÅŸÄ±nÄ± seÃ§
    void setAckMode(AckMode m) { mode_ = m; }

    // RPC Ã§aÄŸrÄ±sÄ± Ã§erÃ§evesini hazÄ±rla ve gÃ¶nder
    void send(const std::string& method, const std::vector<uint8_t>& payload);


    // Sunucudan gelen DATAâ€™larÄ± oku
    const std::vector<std::vector<uint8_t>>& received() const { return recv_; }

    void sendDuplicateLast() {
        if (!sentMessages.empty()) {
            transport_.onRawFrameFromClient(sentMessages.back());
        }
    }

    void resendLast() {
        sendDuplicateLast(); // alias
    }

private:
    // gerÃ§ekte â€œkabukâ€ Ã§erÃ§eveyi alÄ±p serverâ€™a besleyen fonksiyon
    void sendRaw(const std::vector<uint8_t>& frame) {
        transport_.onRawFrameFromClient(frame);
    }

    // WebSocketTransportâ€™dan gelen her frameâ€™i ele al
    void onServerFrame(const std::vector<uint8_t>& buf);


    WebSocketTransport& transport_;
    AckMode mode_;
    bool firstAckDropped_;
    std::vector<std::vector<uint8_t>> recv_;
    std::vector<std::vector<uint8_t>> sentMessages;  // âœ… eksik field
};

} // namespace binaryrpc
