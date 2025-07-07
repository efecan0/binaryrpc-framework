#include "binaryrpc/core/app.hpp"
#include "../include/binaryrpc/transports/websocket/websocket_transport.hpp"
#include "binaryrpc/core/util/qos.hpp"
#include "binaryrpc/core/session/session_manager.hpp"
#include <iostream>

using namespace binaryrpc;

int main() {
    Logger::inst().setLevel(LogLevel::Debug);
    App& app = App::getInstance();
    auto ws = std::make_unique<WebSocketTransport>(app.getSessionManager(), /*idleTimeoutSec=*/60);

    ReliableOptions opts;
    opts.level = QoSLevel::None;

    ws->setReliable(opts);


    app.setTransport(std::move(ws));

    app.use([](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW1] çalıştı\n";
        s.set("step1", true);
        next();
    });

    app.use([](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW2] çalıştı\n";
        bool step1 = s.get<bool>("step1");
        if (!step1) throw std::runtime_error("Middleware1 atlanmış!");
        s.set("step2", true);
        next();
    });

    app.use([](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[Global MW] çalıştı\n";
        s.set("global", true);
        next();
    });

    app.useFor("login", [](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW for login] çalıştı\n";
        s.set("loginMW", true);
        next();
    });

    app.useForMulti({"login", "test.middleware"}, [](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW for login & test.middleware] çalıştı\n";
        s.set("multiMW", true);
        next();
    });


    app.registerRPC("login", [&](const std::vector<uint8_t>& payload, RpcContext& ctx) {
        std::cout << "[Handler] login çağrıldı\n";
        bool global = ctx.session().get<bool>("global");
        bool loginMW = ctx.session().get<bool>("loginMW");
        bool multiMW = ctx.session().get<bool>("multiMW");

        if (!global || !loginMW || !multiMW)
            throw std::runtime_error("Login middleware zinciri incomplete!");

        std::string response = "login all middlewares passed!";
        std::vector<uint8_t> payload_vec(response.begin(), response.end());
        ctx.reply(app.getProtocol()->serialize("login", payload_vec));
    });

    app.registerRPC("test.middleware", [&](const std::vector<uint8_t>& payload, RpcContext& ctx) {
        std::cout << "[Handler] test.middleware çağrıldı\n";
        bool global = ctx.session().get<bool>("global");
        bool loginMW = ctx.session().get<bool>("loginMW");  // burada çalışmamalı
        bool multiMW = ctx.session().get<bool>("multiMW");

        if (!global || loginMW || !multiMW)  // loginMW çalışmamalı!
            throw std::runtime_error("test.middleware middleware zinciri incomplete!");

        std::string response = "test.middleware all middlewares passed!";
        std::vector<uint8_t> payload_vec(response.begin(), response.end());
        ctx.reply(app.getProtocol()->serialize("test.middleware", payload_vec));
    });


    // Bu middleware intentionally next() çağırmamalı (zinciri durduracak)
    app.useFor("stuck.method", [](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW stuck] çalıştı ama next() yok!\n";
        // next() çağırmıyoruz → zincir burada durmalı
    });

    // Bu middleware exception atıyor
    app.useFor("throw.method", [](Session& s, const std::string&, std::vector<uint8_t>&, NextFunc next) {
        std::cout << "[MW throw] çalıştı ve exception fırlatıyor!\n";
        throw std::runtime_error("MW throw exception!");
    });

    app.registerRPC("stuck.method", [&](const std::vector<uint8_t>& payload, RpcContext& ctx) {
        std::cout << "[Handler] stuck.method çağrıldı (BU ÇAĞRILMAMALI)\n";
        std::string response = "should not reach here!";
        std::vector<uint8_t> payload_vec(response.begin(), response.end());
        ctx.reply(app.getProtocol()->serialize("stuck.method", payload_vec));
    });

    app.registerRPC("throw.method", [&](const std::vector<uint8_t>& payload, RpcContext& ctx) {
        std::cout << "[Handler] throw.method çağrıldı (BU ÇAĞRILMAMALI)\n";
        std::string response = "should not reach here!";
        std::vector<uint8_t> payload_vec(response.begin(), response.end());
        ctx.reply(app.getProtocol()->serialize("throw.method", payload_vec));
    });






    app.run(9000);
    std::cout << "Server listening on port 9000\n";

    std::cin.get();
    return 1;
}
