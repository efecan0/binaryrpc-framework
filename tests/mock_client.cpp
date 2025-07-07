#include "mock_client.hpp"
#include <cstring>

using namespace binaryrpc;

void MockClient::send(const std::string& meth, const std::vector<uint8_t>& pl) {
    std::vector<uint8_t> frame;
    frame.push_back(uint8_t(FrameType::FRAME_DATA));
    uint64_t id = 1;                      // testte sabit id
    for (int i = 7; i >= 0; --i)          // bigâ€‘endian
        frame.push_back(uint8_t(id >> (i*8)));
    frame.insert(frame.end(), meth.begin(), meth.end());
    frame.push_back(':');
    frame.insert(frame.end(), pl.begin(), pl.end());
    sendRaw(frame);
}

void MockClient::onServerFrame(const std::vector<uint8_t>& buf) {
    FrameType t = FrameType(buf[0]);
    if (t == FrameType::FRAME_ACK) {
        if (mode_ == AckMode::DropFirst && !firstAckDropped_) {
            firstAckDropped_ = true;          // drop
            return;
        }
        // Normal: teslim et -> hiÃ§bir ÅŸey yapmaya gerek yok
    } else if (t == FrameType::FRAME_DATA) {
        // id atla
        recv_.emplace_back(buf.begin()+1+8, buf.end());
        // Otomatik ACK
        std::vector<uint8_t> ack{ uint8_t(FrameType::FRAME_ACK) };
        ack.insert(ack.end(), buf.begin()+1, buf.begin()+1+8);
        sendRaw(ack);
    }
}
