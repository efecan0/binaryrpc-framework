#include "binaryrpc/core/protocol/msgpack_protocol.hpp"
#include "binaryrpc/core/interfaces/iprotocol.hpp"
#include "binaryrpc/core/util/error_types.hpp" // Corrected path


namespace binaryrpc {

    //------------------------------------------------------------------------------  
    // Safe narrowing conversion: size_t → uint32_t  
    // Returns false if src > UINT32_MAX  
    template<typename S>
    [[nodiscard]] static bool safe_u32(S src, uint32_t& dst) noexcept {
        if constexpr (std::is_unsigned_v<S>) {
            if (src > std::numeric_limits<uint32_t>::max()) return false;
        }
        else {
            if (src < 0 || static_cast<uint64_t>(src) > std::numeric_limits<uint32_t>::max()) return false;
        }
        dst = static_cast<uint32_t>(src);
        return true;
    }
    //------------------------------------------------------------------------------

    ParsedRequest MsgPackProtocol::parse(const std::vector<uint8_t>& data) {
        ParsedRequest req;

        try {
            msgpack::object_handle oh =
                msgpack::unpack(reinterpret_cast<const char*>(data.data()),
                    data.size());
            msgpack::object obj = oh.get();
            if (obj.type != msgpack::type::MAP) return req;

            for (uint32_t i = 0; i < obj.via.map.size; ++i) {
                auto& kv = obj.via.map.ptr[i];
                std::string key; kv.key.convert(key);

                /* ---- method ---- */
                if (key == "method") {
                    if (kv.val.type == msgpack::type::STR) {
                        kv.val.convert(req.methodName);
                    }
                    else if (kv.val.type == msgpack::type::BIN) {
                        req.methodName.assign(
                            kv.val.via.bin.ptr,
                            kv.val.via.bin.ptr + kv.val.via.bin.size);
                    }
                }
                /* ---- payload ---- */
                else if (key == "payload") {
                    if (kv.val.type == msgpack::type::MAP) {
                        // MAP'i binary'e çevir
                        msgpack::sbuffer sbuf;
                        msgpack::pack(sbuf, kv.val);
                        req.payload.assign(sbuf.data(), sbuf.data() + sbuf.size());
                    }
                    else if (kv.val.type == msgpack::type::BIN) {
                        req.payload.assign(
                            reinterpret_cast<const uint8_t*>(kv.val.via.bin.ptr),
                            reinterpret_cast<const uint8_t*>(kv.val.via.bin.ptr)
                            + kv.val.via.bin.size);
                    }
                    else if (kv.val.type == msgpack::type::STR) {
                        std::string s; kv.val.convert(s);
                        req.payload.assign(s.begin(), s.end());
                    }
                }
            }
        }
        catch (...) {}
        return req;
    }

    std::vector<uint8_t> MsgPackProtocol::serialize(
        const std::string& method,
        const std::vector<uint8_t>& payload)
    {
        msgpack::sbuffer buf;
        msgpack::packer<msgpack::sbuffer> pk(&buf);

        pk.pack_map(2);
        pk.pack(std::string("method"));
        pk.pack(method);

        pk.pack(std::string("payload"));
        {
            uint32_t len32;
            if (!safe_u32(payload.size(), len32)) {
                throw std::overflow_error("MsgPackProtocol::serialize: payload size exceeds 4 GiB");
            }
            pk.pack_bin(len32);
            pk.pack_bin_body(reinterpret_cast<const char*>(payload.data()), len32);
        }

        return { buf.data(), buf.data() + buf.size() };
    }

    std::vector<uint8_t> MsgPackProtocol::serializeError(const ErrorObj& e) {
        msgpack::sbuffer buf;
        msgpack::packer<msgpack::sbuffer> pk(&buf);

        const bool hasData = !e.data.empty();
        pk.pack_map(hasData ? 3 : 2);
        pk.pack("code"); pk.pack(static_cast<int>(e.code));
        pk.pack("msg");  pk.pack(e.msg);
        if (hasData) {
            pk.pack("data");
            {
                uint32_t len32;
                if (!safe_u32(e.data.size(), len32)) {
                    throw std::overflow_error("MsgPackProtocol::serializeError: data size exceeds 4 GiB");
                }
                pk.pack_bin(len32);
                pk.pack_bin_body(reinterpret_cast<const char*>(e.data.data()), len32);
            }
        }
        return { buf.data(), buf.data() + buf.size() };
    }
}
