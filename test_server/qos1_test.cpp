// test_server.cpp
#include "binaryrpc/core/app.hpp"
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/util/qos.hpp"
#include "binaryrpc/core/strategies/linear_backoff.hpp"
#include "binaryrpc/core/framework_api.hpp"
#include "binaryrpc/core/util/logger.hpp"
#include <sstream>

using namespace binaryrpc;

int main(){
    Logger::inst().setLevel(LogLevel::Debug);
    App& app = App::getInstance();
    SessionManager& sm = app.getSessionManager();
    auto ws = std::make_unique<WebSocketTransport>(sm, /*idleTimeoutSec=*/30);
    ReliableOptions opts;
    opts.level         = QoSLevel::AtLeastOnce;
    opts.baseRetryMs   = 50;
    opts.maxRetry      = 3;
    opts.maxBackoffMs  = 200;
    opts.sessionTtlMs = 3'000;
    opts.backoffStrategy = std::make_shared<LinearBackoff>(
        std::chrono::milliseconds(opts.baseRetryMs),
        std::chrono::milliseconds(opts.maxBackoffMs)
        );

    ws->setReliable(opts);
    app.setTransport(std::move(ws));

    FrameworkAPI api(&app.getSessionManager(), app.getTransport());


    app.registerRPC("echo", [](const std::vector<uint8_t>& req, RpcContext& ctx) {
        ctx.reply(req);
    });

    app.registerRPC("counter",
        [&api](const std::vector<uint8_t> req, RpcContext& ctx)
        {
            LOG_DEBUG("[Counter RPC] Received request: " + std::string((char*)req.data(), req.size()));
            
            if (std::string_view((char*)req.data(), req.size()) != "inc") {
                LOG_DEBUG("[Counter RPC] Invalid request, sending empty response");
                ctx.reply({});
                return;
            }

            const std::string sid = ctx.session().id();
            const std::string key = "_cnt";
            LOG_DEBUG("[Counter RPC] Processing request for session " + sid);

            /* ---- GET as int ---- */
            int val = 0;
            if (auto opt = api.getField<int>(sid, key); opt) {
                val = *opt;
                LOG_DEBUG("[Counter RPC] Got existing value: " + std::to_string(val));
            } else {
                LOG_DEBUG("[Counter RPC] No existing value, starting from 0");
            }

            ++val;
            LOG_DEBUG("[Counter RPC] Incremented value to: " + std::to_string(val));

            /* ---- SET as int ---- */
            bool success = api.setField<int>(sid, key, val, /*indexed=*/false);
            if (!success) {
                LOG_ERROR("[Counter RPC] Failed to set counter state for session " + sid);
                ctx.reply({});
                return;
            }
            LOG_DEBUG("[Counter RPC] Successfully saved new value: " + std::to_string(val));

            /* ---- REPLY ---- */
            std::string txt = std::to_string(val);
            LOG_DEBUG("[Counter RPC] Sending response: " + txt);
            ctx.reply({ txt.begin(), txt.end() });
        });

    // Premium kullanıcı testi için yeni RPC'ler
    app.registerRPC("login", [&](auto const& req, RpcContext& ctx) {
        std::string p(req.begin(), req.end());
        auto pos = p.find(':');
        if (pos == std::string::npos) return;

        std::string user = p.substr(0, pos);
        std::string role = p.substr(pos + 1);

        LOG_INFO("User logged in: " + user + " with role: " + role);

        // Kullanıcı bilgilerini kaydet
        api.setField(ctx.session().id(), "username", user, /*indexed=*/true);
        api.setField(ctx.session().id(), "role", role, /*indexed=*/true);
        
        // X kullanıcısı için premium özelliğini ekle
        if (user == "X") {
            api.setField(ctx.session().id(), "premium", std::string("1"), /*indexed=*/true);
            LOG_INFO("Set premium=true for user X");
        }

        ctx.reply({});
    });

    app.registerRPC("sendToPremium", [&](auto const& req, RpcContext& ctx) {
        std::string message(req.begin(), req.end());
        LOG_INFO("Sending message to premium users: " + message);

        // Premium kullanıcıları bul
        auto premiumUsers = api.findBy("premium", "1");
        LOG_INFO("Found " + std::to_string(premiumUsers.size()) + " premium users");

        // Her premium kullanıcıya gönder
        for (auto& session : premiumUsers) {
            api.sendToSession(session, req);
            LOG_INFO("Message sent to premium user: " + session->id());
        }

        ctx.reply({});
    });

    app.run(9010);

    std::cin.get();
    return 0;
}
