#include <catch2/catch_all.hpp>
#include <thread>                       //  <-- eksikti
#include <vector>
#include <atomic>
#include "binaryrpc/core/session/generic_index.hpp"

using namespace binaryrpc;
static std::string sid(int i) { return "s" + std::to_string(i); }

TEST_CASE("add() tek oturum â€“ lookup Ã§alÄ±ÅŸÄ±r", "[index]") {
    GenericIndex gi;
    gi.add(sid(1), "room", "lobby");

    auto set = gi.find("room", "lobby");
    REQUIRE(set.size() == 1);
    REQUIRE(set.count(sid(1)) == 1);
}

TEST_CASE("add() duplicate id/value idempotent", "[index]") {
    GenericIndex gi;
    gi.add(sid(1), "tenant", "5");
    gi.add(sid(1), "tenant", "5");
    REQUIRE(gi.find("tenant","5").size() == 1);
}

TEST_CASE("overwrite removes old mapping", "[index]") {
    GenericIndex gi;
    gi.add(sid(1), "tier", "silver");
    gi.add(sid(1), "tier", "gold");

    REQUIRE(gi.find("tier","silver").empty());
    auto gold = gi.find("tier","gold");
    REQUIRE(gold.size() == 1);
    REQUIRE(gold.count(sid(1)) == 1);
}

TEST_CASE("multiple sessions same key", "[index]") {
    GenericIndex gi;
    for (int i=1;i<=5;++i) gi.add(sid(i),"group","A");
    REQUIRE(gi.find("group","A").size() == 5);
}

TEST_CASE("remove(id) cleans all mappings", "[index]") {
    GenericIndex gi;
    gi.add("x","a","1");
    gi.add("x","b","2");

    gi.remove("x");                        // GenericIndex::remove(sid)
    REQUIRE(gi.find("a","1").empty());
    REQUIRE(gi.find("b","2").empty());
}


TEST_CASE("large volume remains consistent", "[index][stress]") {
    GenericIndex gi;
    constexpr int N = 10000;
    for (int i=0;i<N;++i)
        gi.add(sid(i),"bucket", std::to_string(i%10));
    REQUIRE(gi.find("bucket","3").size() == N/10);
}
TEST_CASE("threadâ€‘safe add/remove in two phases", "[index][concurrency]") {
    GenericIndex gi;
    constexpr int THREADS = 16, OPS = 500;

    {
        std::vector<std::thread> writers;
        for (int t=0;t<THREADS;++t)
            writers.emplace_back([&,t]{
                for (int i=0;i<OPS;++i)
                    gi.add(sid(t*OPS+i),"k","v");
            });
        for (auto& th : writers) th.join();
    }

    for (int i=0;i<THREADS*OPS;i+=2)
        gi.remove(sid(i));

    REQUIRE(gi.find("k","v").size() == THREADS*OPS/2);
}
