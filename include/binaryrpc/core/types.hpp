#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace binaryrpc {

    class Session; // Forward-declaration
    class RpcContext; // Forward-declaration
    using NextFunc = std::function<void()>;
    using Middleware = std::function<void(Session&, const std::string&, std::vector<uint8_t>&, NextFunc)>;
    using RpcContextHandler =
        std::function<void(const std::vector<uint8_t>&, RpcContext&)>;

    enum class SendMode {
        Client,
        Broadcast
    };

}
