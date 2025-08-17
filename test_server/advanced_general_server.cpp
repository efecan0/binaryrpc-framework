#include "binaryrpc/core/rpc/rpc_context.hpp"
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <iostream>
#include "binaryrpc/core/app.hpp"
#include "binaryrpc/core/framework_api.hpp"
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/transports/websocket/websocket_transport.hpp"

using namespace binaryrpc;

int main() {   

    App& app = App::getInstance();

    SessionManager& sm = app.getSessionManager();
    auto ws = std::make_unique<WebSocketTransport>(sm, /*idleTimeoutSec=*/30);

    ReliableOptions opts;
    opts.level = QoSLevel::None;

    ws->setReliable(opts);

    app.setTransport(std::move(ws));

    FrameworkAPI api(&sm, app.getTransport());

    // === Middleware: log session ID ve requestCount ===
    app.use([](Session& s, const std::string& method, std::vector<uint8_t>& payload, NextFunc next) {
        int count = 0;
        try { count = s.get<int>("requestCount"); } catch (...) {}
        s.set<int>("requestCount", count + 1);
        LOG_INFO("[MW] Session " + s.id() + " requestCount=" + std::to_string(count + 1));
        next();
    });

    // === RPC: set_data_indexed_false ===
    app.registerRPC("set_data_indexed_false", [&api](const std::vector<uint8_t>& req, RpcContext& ctx) {
        std::string sid = ctx.session().id();
        std::string val(req.begin(), req.end());
        api.setField<std::string>(sid, "username", val, false);
        LOG_INFO("[RPC] set_data_indexed_false: " + val);
        ctx.reply(std::vector<uint8_t>{'O','K'});
    });

    // === RPC: set_data_indexed_true ===
    app.registerRPC("set_data_indexed_true", [&api](const std::vector<uint8_t>& req, RpcContext& ctx) {
        std::string sid = ctx.session().id();
        std::string val(req.begin(), req.end());
        api.setField<std::string>(sid, "username", val, true);
        LOG_INFO("[RPC] set_data_indexed_true: " + val);
        ctx.reply(std::vector<uint8_t>{'O','K'});
    });

    // === RPC: get_data ===
    app.registerRPC("get_data", [&api](const std::vector<uint8_t>&, RpcContext& ctx) {
        std::string sid = ctx.session().id();
        auto val = api.getField<std::string>(sid, "username");
        if (val.has_value()) {
            LOG_INFO("[RPC] get_data: found " + val.value());
            std::vector<uint8_t> out(val->begin(), val->end());
            ctx.reply(out);
        } else {
            LOG_WARN("[RPC] get_data: no value");
            ctx.reply({'N','O','N','E'});
        }
    });

    // === RPC: find_by ===
    app.registerRPC("find_by", [&api](const std::vector<uint8_t>& req, RpcContext& ctx) {
        std::string key = "username";
        std::string val(req.begin(), req.end());
        auto sessions = api.findBy(key, val); // <<< template argÃ¼man yok!
        std::string ids;
        for (const auto& s : sessions) {
            ids += s->id() + ",";
        }
        if (!ids.empty()) ids.pop_back(); // remove last comma
        LOG_INFO("[RPC] find_by: " + ids);
        std::vector<uint8_t> out(ids.begin(), ids.end());
        ctx.reply(out);
    });



    app.run(9000);

    std::cin.get();
    return 0;
}
