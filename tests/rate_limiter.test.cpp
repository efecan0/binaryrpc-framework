#include <catch2/catch_all.hpp>
#include <chrono>
#include "binaryrpc/middlewares/rate_limiter.hpp"
#include "binaryrpc/core/session/session.hpp"

using namespace binaryrpc;

// Yardımcı fonksiyon: Test için Session oluşturur
static Session makeSession(const std::string& id) {
    ClientIdentity ident;
    ident.clientId = id;
    ident.deviceId = 0;
    return Session(ident, id);
}

// Helper: invoke limiter and count how many times `next` is called.
static int invokeLimiter(const std::function<void(Session&, NextFunc)>& mw,
                         Session& s,
                         int calls)
{
    int cnt = 0;
    NextFunc nf = [&]{ ++cnt; };
    for (int i = 0; i < calls; ++i)
        mw(s, nf);
    return cnt;
}

TEST_CASE("RateLimiter: burst capacity enforced", "[rate]") {
    // qps=1, burst=2
    auto mw = rateLimiter(1,2);
    Session s = makeSession("A");

    // İlk iki çağrı izinli, üçüncü reddedilir
    int ok = invokeLimiter(mw, s, 3);
    REQUIRE(ok == 2);
}

TEST_CASE("RateLimiter: refill after 1 second", "[rate]") {
    auto mw = rateLimiter(1,2);
    Session s = makeSession("B");

    // Tüketelim
    invokeLimiter(mw, s, 2);
    auto b = s.get<RateBucket*>("_bucket");
    REQUIRE(b != nullptr);

    // 1 saniye geriye al: 1 token eklenmeli
    b->last -= std::chrono::seconds(1);
    int ok1 = invokeLimiter(mw, s, 2);
    REQUIRE(ok1 == 1);  // yalnızca 1 token refill edildi

    // Tekrar 2 saniye geriye al: burst geri gelmeli
    b->last -= std::chrono::seconds(2);
    int ok2 = invokeLimiter(mw, s, 3);
    REQUIRE(ok2 == 2);  // burst=2 token
}

TEST_CASE("RateLimiter: multiple sessions isolated", "[rate]") {
    auto mw = rateLimiter(1,3);
    Session s1 = makeSession("S1"), s2 = makeSession("S2");

    int c1 = invokeLimiter(mw, s1, 5);
    int c2 = invokeLimiter(mw, s2, 5);
    REQUIRE(c1 == 3);
    REQUIRE(c2 == 3);
}

TEST_CASE("RateLimiter: default burst parameter", "[rate]") {
    // burst default = 2
    auto mw = rateLimiter(5);
    Session s = makeSession("D");
    int ok = invokeLimiter(mw, s, 4);
    REQUIRE(ok == 2);
}

TEST_CASE("RateLimiter: zero requests do nothing", "[rate]") {
    auto mw = rateLimiter(10,5);
    Session s = makeSession("Z");
    int ok = invokeLimiter(mw, s, 0);
    REQUIRE(ok == 0);
}