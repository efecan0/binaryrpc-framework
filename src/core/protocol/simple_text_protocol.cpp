#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include "binaryrpc/core/util/error_types.hpp"
#include <string>

namespace binaryrpc {

    ParsedRequest SimpleTextProtocol::parse(const std::vector<uint8_t>& data) {
        LOG_DEBUG("[SimpleTextProtocol::parse] Gelen veri: " + std::string(data.begin(), data.end()));
        std::string s(data.begin(), data.end());
        auto pos = s.find(':');
        if (pos == std::string::npos) return { "", {} };

        ParsedRequest req;
        req.methodName = s.substr(0, pos);
        std::string param = s.substr(pos + 1);
        req.payload.assign(param.begin(), param.end());
        return req;
    }

    std::vector<uint8_t> SimpleTextProtocol::serialize(const std::string& method,
        const std::vector<uint8_t>& payload) {
        std::string out = method + ":";
        out.insert(out.end(), payload.begin(), payload.end());
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    std::vector<uint8_t> SimpleTextProtocol::serializeError(const ErrorObj& e) {
        std::string s = "error:" + std::to_string(static_cast<int>(e.code)) + ":" + e.msg;
        return { s.begin(), s.end() };
    }


}
