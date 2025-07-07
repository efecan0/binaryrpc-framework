#include <catch2/catch_all.hpp>
#include "binaryrpc/core/middleware/middleware_chain.hpp"
#include "binaryrpc/core/session/session.hpp"
#include <vector>
#include <string>

using namespace binaryrpc;

struct Flag { bool called = false; };

// Yardımcı fonksiyon: Test için Session oluşturur
Session makeSession(const std::string& id) {
    ClientIdentity ident;
    ident.clientId = id;
    ident.deviceId = 0;
    return Session(ident, id);
}

// ------------------------------------------------------------------
// 1) add( )  sırası korunur ve execute() true döner
// ------------------------------------------------------------------
TEST_CASE("Middleware executed in registration order", "[mw]") {
    MiddlewareChain chain;
    std::vector<int> order;

    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { order.push_back(1); next(); });
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { order.push_back(2); next(); });
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { order.push_back(3); next(); });

    Session s = makeSession("X");
    std::vector<uint8_t> payload;
    bool ok = chain.execute(s, "echo", payload);

    REQUIRE(ok);
    REQUIRE(order == std::vector<int>{1, 2, 3});
}

// ------------------------------------------------------------------
// 2) next() çağrılmazsa zincir kesilir ve execute() false
// ------------------------------------------------------------------
TEST_CASE("Short-circuit when next() isn't called", "[mw]") {
    MiddlewareChain chain;
    Flag f1, f2;

    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { f1.called = true; /* next yok */ });
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { f2.called = true; });

    Session s = makeSession("Y");
    std::vector<uint8_t> payload;
    bool ok = chain.execute(s, "any", payload);

    REQUIRE_FALSE(ok);
    REQUIRE(f1.called);
    REQUIRE_FALSE(f2.called);
}

// ------------------------------------------------------------------
// 3) addFor() sadece ilgili metoda uygulanır
// ------------------------------------------------------------------
TEST_CASE("addFor attaches to given method only", "[mw]") {
    MiddlewareChain chain;
    Flag global, fooOnly;

    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { global.called = true; next(); });
    chain.addFor("foo", [&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { fooOnly.called = true; });

    Session s = makeSession("Z");
    std::vector<uint8_t> payload;

    chain.execute(s, "bar", payload);
    REQUIRE(global.called);
    REQUIRE_FALSE(fooOnly.called);

    global.called = false; fooOnly.called = false;

    chain.execute(s, "foo", payload);
    REQUIRE(global.called);
    REQUIRE(fooOnly.called);
}

// ------------------------------------------------------------------
// 4) Exception'i zincir kısa keser; execute() false, sonraki MW çalışmaz
// ------------------------------------------------------------------
TEST_CASE("Exception inside middleware short-circuits chain", "[mw]") {
    MiddlewareChain chain;
    bool reached = false;

    chain.add([](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { throw std::runtime_error("boom"); });
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { reached = true; });   // çalışmamalı

    Session s = makeSession("E");
    std::vector<uint8_t> payload;
    bool ok = chain.execute(s, "x", payload);

    REQUIRE_FALSE(ok);        // zincir tamamlanmadı
    REQUIRE_FALSE(reached);   // sonraki MW'e hiç gelinmedi
}

// ------------------------------------------------------------------
// 5) Bir MW'yi birden fazla metoda eklemek (çoklu addFor)
// ------------------------------------------------------------------
TEST_CASE("addForMulti attaches same MW to many methods", "[mw]") {
    MiddlewareChain chain;
    int counter = 0;

    auto incr = [&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { ++counter; next(); };
    chain.addFor("foo", incr);
    chain.addFor("bar", incr);

    Session s = makeSession("A");
    std::vector<uint8_t> payload;
    chain.execute(s, "foo", payload);
    chain.execute(s, "bar", payload);

    REQUIRE(counter == 2);
}

// ------------------------------------------------------------------
// 6) Global + scoped middleware birleşik sırada yürür
// ------------------------------------------------------------------
TEST_CASE("Global and scoped MW execute in combined order", "[mw]") {
    MiddlewareChain chain;
    std::vector<std::string> trace;

    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { trace.push_back("G1"); next(); });
    chain.addFor("ping", [&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { trace.push_back("S1"); next(); });
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { trace.push_back("G2");     next();     });

    Session s = makeSession("B");
    std::vector<uint8_t> payload;
    chain.execute(s, "ping", payload);

    // Globaller önce, scoped en sonda
    REQUIRE(trace == std::vector<std::string>{"G1", "G2", "S1"});
}

// ------------------------------------------------------------------
// 7) Scoped MW yoksa sadece global çalışır, execute true döner
// ------------------------------------------------------------------
TEST_CASE("No scoped MW falls back to global only", "[mw]") {
    MiddlewareChain chain;
    bool ran = false;
    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { ran = true; next(); });

    Session s = makeSession("C");
    std::vector<uint8_t> payload;
    bool ok = chain.execute(s, "nonexistent", payload);

    REQUIRE(ran);
    REQUIRE(ok);
}

// ------------------------------------------------------------------
// 8) Scoped MW zinciri keserse execute false
// ------------------------------------------------------------------
TEST_CASE("Scoped MW can short-circuit chain", "[mw]") {
    MiddlewareChain chain;
    bool g = false, s = false;

    chain.add([&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc next) { g = true; next(); });
    chain.addFor("halt", [&](Session&, const std::string&, std::vector<uint8_t>&, NextFunc) { s = true; /* next yok */ });

    Session sx = makeSession("D");
    std::vector<uint8_t> payload;
    bool ok = chain.execute(sx, "halt", payload);

    REQUIRE(g);
    REQUIRE(s);
    REQUIRE_FALSE(ok);
}