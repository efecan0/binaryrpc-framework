#include "binaryrpc/core/app.hpp"
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/util/qos.hpp"
#include "binaryrpc/core/framework_api.hpp"
#include <iostream>

using namespace binaryrpc;

int main() {
    Logger::inst().setLevel(LogLevel::Debug);

    App& app = App::getInstance();

    auto ws = std::make_unique<WebSocketTransport>(app.getSessionManager(), 60);

    ReliableOptions opts;
    opts.level = QoSLevel::None;
    opts.sessionTtlMs = 1;

    ws->setReliable(opts);


    app.setTransport(std::move(ws));

    // ---- Frameworkâ€‘level API ----
    FrameworkAPI api(&app.getSessionManager(), app.getTransport());

    /* nonâ€‘indexed set/get */
    app.registerRPC("set.nonidx", [&](const std::vector<uint8_t>& p, RpcContext& ctx){
        std::string sid = ctx.session().id();
        std::string val(p.begin(), p.end());
        api.setField<std::string>(sid, "nonidx", val, /*indexed=*/false);
        ctx.reply(app.getProtocol()->serialize("set.nonidx",
                                                               {'o','k'}));
    });

    app.registerRPC("get.nonidx", [&](const std::vector<uint8_t>&, RpcContext& ctx){
        auto val = api.getField<std::string>(ctx.session().id(), "nonidx")
        .value_or("missing");
        ctx.reply(app.getProtocol()->serialize("get.nonidx",
                                                               {val.begin(), val.end()}));
    });

    /* indexed set / find */
    app.registerRPC("set.idx", [&](const std::vector<uint8_t>& p, RpcContext& ctx){
        std::string sid = ctx.session().id();
        std::string city(p.begin(), p.end());
        api.setField<std::string>(sid, "city", city, /*indexed=*/true);
        ctx.reply(app.getProtocol()->serialize("set.idx",
                                                               {'o','k'}));
    });

    app.registerRPC("find.city", [&](const std::vector<uint8_t>& p, RpcContext& ctx){
        std::string city(p.begin(), p.end());
        auto v  = api.findBy("city", city);
        std::string resp = std::to_string(v.size());
        ctx.reply(app.getProtocol()->serialize("find.city",
                                                               {resp.begin(), resp.end()}));
    });

    /* session enumeration / disconnect */
    app.registerRPC("list.sessions", [&](const std::vector<uint8_t>&, RpcContext& ctx){
        auto n = api.listSessionIds().size();
        std::string resp = std::to_string(n);
        ctx.reply(app.getProtocol()->serialize("list.sessions",
                                                               {resp.begin(), resp.end()}));
    });

    app.registerRPC("bye", [&](const std::vector<uint8_t>&, RpcContext& ctx){
        api.disconnect(ctx.session().id());    // istemciyi kopar
    });

    app.run(9000);
    std::cout << "[server] listening on :9000\n";
    std::cin.get();
    return 1;
}
