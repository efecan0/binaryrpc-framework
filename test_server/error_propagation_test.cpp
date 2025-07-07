#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/framework_api.hpp"
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"

using namespace binaryrpc;

int main() {
    App& app = App::getInstance();
    SessionManager& sm = app.getSessionManager();
    auto ws = std::make_unique<WebSocketTransport>(sm, /*idleTimeoutSec=*/30);

    ReliableOptions opts;
    opts.level = QoSLevel::None;

    ws->setReliable(opts);

    app.setTransport(std::move(ws));

    app.registerRPC("echo", [](const std::vector<uint8_t>& req, RpcContext& ctx) {
        ctx.reply(req);
    });

    app.registerRPC("throwing_handler", [](auto req, auto ctx) {
        try {
            throw std::runtime_error("intentional error!");
        } catch (const std::exception& ex) {
            std::string err_msg = "error:Handler exception: ";
            err_msg += ex.what();
            ctx.reply(std::vector<uint8_t>(err_msg.begin(), err_msg.end()));
        }
    });


    app.run(9002);


    std::cin.get();
    return 0;
}
