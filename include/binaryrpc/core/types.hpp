#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace binaryrpc {

    using NextFunc = std::function<void()>;

    enum class SendMode {
        Client,
        Broadcast
    };

}
