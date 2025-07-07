#include <catch2/catch_all.hpp>
#include <numeric>                       // for iota (bulk test)
#include <msgpack.hpp>                   // for unpacking error maps
#include "binaryrpc/core/protocol/simple_text_protocol.hpp"
#include "binaryrpc/core/protocol/msgpack_protocol.hpp"
#include "binaryrpc/core/util/error_types.hpp"

using namespace binaryrpc;

TEST_CASE("SimpleTextProtocol: round-trip small payload", "[protocol][simpletext]") {
    SimpleTextProtocol p;
    std::vector<uint8_t> in{1,2,3};
    auto data = p.serialize("foo", in);
    auto req  = p.parse(data);

    REQUIRE(req.methodName == "foo");
    REQUIRE(req.payload    == in);
}

TEST_CASE("SimpleTextProtocol: empty payload", "[protocol][simpletext]") {
    SimpleTextProtocol p;
    auto data = p.serialize("bar", {});
    auto req  = p.parse(data);

    REQUIRE(req.methodName == "bar");
    REQUIRE(req.payload.empty());
}

TEST_CASE("SimpleTextProtocol: invalid frame â†’ empty request", "[protocol][simpletext]") {
    SimpleTextProtocol p;
    std::vector<uint8_t> bad{ 'x','y','z' };
    auto req = p.parse(bad);

    REQUIRE(req.methodName.empty());
    REQUIRE(req.payload.empty());
}

TEST_CASE("SimpleTextProtocol: serializeError format", "[protocol][simpletext]") {
    SimpleTextProtocol p;
    ErrorObj e{ RpcErr::NotFound, "oops", {4,5} };
    auto err = p.serializeError(e);
    std::string s(err.begin(), err.end());

    REQUIRE(s == "error:3:oops");    // RpcErr::NotFound == 3
}

TEST_CASE("MsgPackProtocol: round-trip small payload", "[protocol][msgpack]") {
    MsgPackProtocol p;
    std::vector<uint8_t> in{5,6,7,8};
    auto data = p.serialize("m", in);
    auto req  = p.parse(data);

    REQUIRE(req.methodName == "m");
    REQUIRE(req.payload    == in);
}

TEST_CASE("MsgPackProtocol: empty payload", "[protocol][msgpack]") {
    MsgPackProtocol p;
    auto data = p.serialize("x", {});
    auto req  = p.parse(data);

    REQUIRE(req.methodName == "x");
    REQUIRE(req.payload.empty());
}

TEST_CASE("MsgPackProtocol: invalid data â†’ empty request", "[protocol][msgpack]") {
    MsgPackProtocol p;
    std::vector<uint8_t> bad{ 0x01, 0x02, 0x03 };
    auto req = p.parse(bad);

    REQUIRE(req.methodName.empty());
    REQUIRE(req.payload.empty());
}

TEST_CASE("MsgPackProtocol: serializeError without data", "[protocol][msgpack]") {
    MsgPackProtocol p;
    ErrorObj e{ RpcErr::Auth, "denied", {} };
    auto buf = p.serializeError(e);

    auto oh  = msgpack::unpack(reinterpret_cast<const char*>(buf.data()), buf.size());
    auto obj = oh.get();
    REQUIRE(obj.type == msgpack::type::MAP);

    bool seenCode=false, seenMsg=false, seenData=false;
    auto& mp = obj.via.map;
    for (uint32_t i = 0; i < mp.size; ++i) {
        auto& kv = mp.ptr[i];
        std::string key; kv.key.convert(key);
        if (key == "code") {
            int code; kv.val.convert(code);
            REQUIRE(code == static_cast<int>(RpcErr::Auth));
            seenCode = true;
        } else if (key == "msg") {
            std::string msg; kv.val.convert(msg);
            REQUIRE(msg == "denied");
            seenMsg = true;
        } else if (key == "data") {
            seenData = true;
        }
    }
    REQUIRE(seenCode);
    REQUIRE(seenMsg);
    REQUIRE_FALSE(seenData);
}

TEST_CASE("MsgPackProtocol: serializeError with data", "[protocol][msgpack]") {
    MsgPackProtocol p;
    ErrorObj e{ RpcErr::Internal, "fail", {9,8,7} };
    auto buf = p.serializeError(e);

    auto oh  = msgpack::unpack(reinterpret_cast<const char*>(buf.data()), buf.size());
    auto obj = oh.get();
    REQUIRE(obj.type == msgpack::type::MAP);

    bool seenCode=false, seenMsg=false, seenData=false;
    auto& mp2 = obj.via.map;
    for (uint32_t i = 0; i < mp2.size; ++i) {
        auto& kv = mp2.ptr[i];
        std::string key; kv.key.convert(key);
        if (key == "code") {
            int code; kv.val.convert(code);
            REQUIRE(code == static_cast<int>(RpcErr::Internal));
            seenCode = true;
        } else if (key == "msg") {
            std::string msg; kv.val.convert(msg);
            REQUIRE(msg == "fail");
            seenMsg = true;
        } else if (key == "data") {
            // bin object
            REQUIRE(kv.val.type == msgpack::type::BIN);
            auto ptr = kv.val.via.bin.ptr;
            auto sz  = kv.val.via.bin.size;
            REQUIRE(std::vector<uint8_t>(ptr, ptr+sz) == e.data);
            seenData = true;
        }
    }
    REQUIRE(seenCode);
    REQUIRE(seenMsg);
    REQUIRE(seenData);
}

// Optional performance check on large payload
TEST_CASE("MsgPackProtocol: large payload round-trip", "[protocol][msgpack][performance]") {
    MsgPackProtocol p;
    constexpr int N = 5000;
    std::vector<uint8_t> in(N);
    std::iota(in.begin(), in.end(), 0);
    auto data = p.serialize("bulk", in);
    auto req  = p.parse(data);

    REQUIRE(req.methodName == "bulk");
    REQUIRE(req.payload.size() == N);
    REQUIRE(req.payload == in);
}

TEST_CASE("SimpleTextProtocol: method with colon", "[protocol][simpletext]") {
    SimpleTextProtocol p;
    // Tek payload olarak 42 verelim
    std::vector<uint8_t> payload{42};
    auto frame = p.serialize("a:b:c", payload);
    auto req   = p.parse(frame);

    // SimpleTextProtocol, ilk ':' kadarÄ±nÄ± methodName, geriye kalanÄ±nÄ± payload yapar.
    REQUIRE(req.methodName == "a");

    // Beklenen payload: "b:c:" karakter dizisi, ardÄ±ndan byte 42
    std::vector<uint8_t> expect;
    for (char ch : std::string("b:c:")) {
        expect.push_back(static_cast<uint8_t>(ch));
    }
    expect.push_back(42);

    REQUIRE(req.payload == expect);
}


// 4) MsgPackProtocol: non-UTF8 methodName
TEST_CASE("MsgPackProtocol: non-UTF8 method names", "[protocol][msgpack]") {
    MsgPackProtocol p;
    // Ã¶rneÄŸin Latin1 byteâ€™larÄ±
    std::string m; m.push_back(char(0xC3)); m.push_back(char(0xA7)); // Ã§
    auto data = p.serialize(m, {});
    auto req  = p.parse(data);
    REQUIRE(req.methodName == m);
}

// 5) Cross-protocol compatibility of payload only
TEST_CASE("Cross-protocol payload passthrough", "[protocol]") {
    SimpleTextProtocol st;
    MsgPackProtocol   mp;

    std::vector<uint8_t> data{1,2,3,4,5};
    // MsgPack ile encode edilip, SimpleText iÃ§ine gÃ¶mÃ¼lÃ¼p Ã§Ä±karÄ±lacak
    auto mpFrame = mp.serialize("x", data);
    auto stFrame = st.serialize("wrap", mpFrame);
    auto req1    = st.parse(stFrame);
    auto req2    = mp.parse(req1.payload);

    REQUIRE(req1.methodName == "wrap");
    REQUIRE(req2.methodName == "x");
    REQUIRE(req2.payload    == data);
}

TEST_CASE("MsgPackProtocol: non-ASCII method names", "[protocol][msgpack]") {
    MsgPackProtocol p;
    // Ã¶rneÄŸin TÃ¼rkÃ§e 'Ã§' karakteri
    std::string name = "Ã§Ã¶zÃ¼m";
    auto frame = p.serialize(name, {});
    auto req   = p.parse(frame);

    REQUIRE(req.methodName == name);
    REQUIRE(req.payload.empty());
}


