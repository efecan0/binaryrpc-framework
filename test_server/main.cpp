// src/main.cpp
#include "binaryrpc/core/rpc/rpc_context.hpp"
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <iostream>
#include "binaryrpc/core/app.hpp"                              // App sÄ±nÄ±fÄ± :contentReference[oaicite:0]{index=0}:contentReference[oaicite:1]{index=1}
#include "binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/util/qos.hpp"

using namespace binaryrpc;

int main() {
    uint16_t port = 9011;
    
    Logger::inst().setLevel(LogLevel::Debug);
    // 1) App nesnesi oluÅŸtur ve WebSocketTransportâ€™u ata
    App& app = App::getInstance();
    auto ws = std::make_unique<WebSocketTransport>(app.getSessionManager(), /*idleTimeoutSec=*/60);

    ReliableOptions opts;
    opts.level = QoSLevel::None;

    ws->setReliable(opts);


    app.setTransport(std::move(ws));

    // 2) Basit bir 'echo' RPC kaydÄ±
    app.registerRPC("echo", [](auto const& req, RpcContext& ctx) {
        // Gelen payload'u birebir geri gÃ¶nder
        ctx.reply(req);
    });

    // 3) Serverâ€™Ä± ayaÄŸa kaldÄ±r
    std::cout << "[Server] Listening on port " << port << "\n";
    app.run(port);

    // (CTRL+C ile durdurulunca stop() Ã§aÄŸrÄ±lacak)
    std::cin.get();
    return 0;
}
