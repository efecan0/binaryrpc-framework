#include <catch2/catch_all.hpp>
#include "binaryrpc/core/session/session_manager.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace binaryrpc;

// Yardımcı fonksiyon: Test için ClientIdentity oluşturur
static ClientIdentity makeIdent(const std::string& id = "test", uint64_t device = 0) {
    ClientIdentity cid;
    cid.clientId = id;
    cid.deviceId = device;
    return cid;
}

TEST_CASE("SessionManager createSession() benzersiz ID üretir", "[session]") {
    SessionManager sm;
    auto s1 = sm.createSession(makeIdent("s1"), 0);
    auto s2 = sm.createSession(makeIdent("s2"), 0);
    REQUIRE(s1 != nullptr);
    REQUIRE(s2 != nullptr);
    REQUIRE(s1->id() != s2->id());
}

TEST_CASE("indexedSet() ikincil indekse ekler ve find() döner", "[session][index]") {
    SessionManager sm;
    auto s = sm.createSession(makeIdent("a"), 0);

    sm.indexedSet(s, "userId", 42);   // int ➜ "42"

    auto ids_ptr = sm.findIndexed("userId", "42");
    REQUIRE(ids_ptr);
    REQUIRE(ids_ptr->size() == 1);
    REQUIRE(ids_ptr->count(s->id()) == 1);
}

TEST_CASE("removeSession() kaydı ve indeksleri temizler", "[session][index]") {
    SessionManager sm;
    auto s = sm.createSession(makeIdent("b"), 0);
    sm.indexedSet(s, "room", std::string("lobby"));

    sm.removeSession(s->id());
    REQUIRE(sm.getSession(s->id()) == nullptr);
    REQUIRE_FALSE(sm.findIndexed("room", "lobby"));
}

TEST_CASE("indexedSet overwrites previous value but maintains consistency", "[session][index]") {
    SessionManager sm;
    auto s = sm.createSession(makeIdent("c"), 0);
    sm.indexedSet(s, "room", std::string("lobby"));
    sm.indexedSet(s, "room", std::string("garden"));   // overwrite

    REQUIRE_FALSE(sm.findIndexed("room", "lobby"));
    auto ids_ptr = sm.findIndexed("room", "garden");
    REQUIRE(ids_ptr);
    REQUIRE(ids_ptr->count(s->id()) == 1);
}

TEST_CASE("find() returns multiple sessions sharing same key", "[session][index]") {
    SessionManager sm;
    auto a = sm.createSession(makeIdent("a1"), 0);
    auto b = sm.createSession(makeIdent("a2"), 0);
    sm.indexedSet(a, "tenant", 5);
    sm.indexedSet(b, "tenant", 5);

    auto ids_ptr = sm.findIndexed("tenant", "5");
    REQUIRE(ids_ptr);
    REQUIRE(ids_ptr->size() == 2);
    REQUIRE(ids_ptr->count(a->id()) == 1);
    REQUIRE(ids_ptr->count(b->id()) == 1);
}

TEST_CASE("removeSession on unknown id is no‑op", "[session]") {
    SessionManager sm;
    REQUIRE_NOTHROW(sm.removeSession("9999"));
}

// 🔄 Concurrency testi – sadeleştirilmiş
TEST_CASE("Concurrent indexedSet is thread‑safe", "[session][concurrency]") {
    SessionManager sm;
    auto s = sm.createSession(makeIdent("conc"), 0);

    constexpr int N = 1000;
    std::atomic<int> done{ 0 };
    std::vector<std::thread> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&, i] {
            sm.indexedSet(s, "counter", i);
            done.fetch_add(1, std::memory_order_relaxed);
            });
    }
    for (auto& t : threads) t.join();

    REQUIRE(done == N);
}


TEST_CASE("indices() snapshot reflects current state after erase", "[session][index]") {
    SessionManager sm;
    auto s = sm.createSession(makeIdent("snap"), 0);
    sm.indexedSet(s, "user", 1);
    sm.removeSession(s->id());

    auto ids_ptr = sm.findIndexed("user", "1");
    REQUIRE_FALSE(ids_ptr);
}