#pragma once

#include <map>
#include <string>
#include <vector>
#include <msgpack.hpp>
#include <nlohmann/json.hpp>

namespace msgpack {
    MSGPACK_API_VERSION_NAMESPACE(v1) {
        template <typename Stream>
        inline packer<Stream>& operator<<(packer<Stream>& o, const std::string& v) {
            o.pack_str(v.size());
            o.pack_str_body(v.data(), v.size());
            return o;
        }

        template <typename Stream>
        inline packer<Stream>& operator<<(packer<Stream>& o, const char* v) {
            o.pack_str(strlen(v));
            o.pack_str_body(v, strlen(v));
            return o;
        }

        // Sabit uzunluklu karakter dizileri için özel tanımlamalar
        template <typename Stream, size_t N>
        inline packer<Stream>& operator<<(packer<Stream>& o, const char (&v)[N]) {
            o.pack_str(N-1);  // N-1 çünkü son karakter null terminator
            o.pack_str_body(v, N-1);
            return o;
        }

        // object sınıfı için özel tanımlamalar
        template <size_t N>
        inline void operator<<(object& o, const char (&v)[N]) {
            o.type = type::STR;
            o.via.str.size = N-1;
            o.via.str.ptr = v;
        }
    }

    MSGPACK_API_VERSION_NAMESPACE(v2) {
        template <typename Stream>
        inline packer<Stream>& operator<<(packer<Stream>& o, const std::string& v) {
            o.pack_str(v.size());
            o.pack_str_body(v.data(), v.size());
            return o;
        }

        template <typename Stream>
        inline packer<Stream>& operator<<(packer<Stream>& o, const char* v) {
            o.pack_str(strlen(v));
            o.pack_str_body(v, strlen(v));
            return o;
        }

        // Sabit uzunluklu karakter dizileri için özel tanımlamalar
        template <typename Stream, size_t N>
        inline packer<Stream>& operator<<(packer<Stream>& o, const char (&v)[N]) {
            o.pack_str(N-1);  // N-1 çünkü son karakter null terminator
            o.pack_str_body(v, N-1);
            return o;
        }

        // object sınıfı için özel tanımlamalar
        template <size_t N>
        inline void operator<<(object& o, const char (&v)[N]) {
            o.type = type::STR;
            o.via.str.size = N-1;
            o.via.str.ptr = v;
        }
    }
}

// Recursive olarak MsgPack objesini JSON'a dönüştüren yardımcı fonksiyon
inline nlohmann::json convertMsgPackToJson(const msgpack::object& obj) {
    switch (obj.type) {
        case msgpack::type::STR: {
            std::string str;
            obj.convert(str);
            return str;
        }
        case msgpack::type::POSITIVE_INTEGER:
        case msgpack::type::NEGATIVE_INTEGER: {
            int64_t num;
            obj.convert(num);
            return num;
        }
        case msgpack::type::FLOAT: {
            double num;
            obj.convert(num);
            return num;
        }
        case msgpack::type::BOOLEAN: {
            bool b;
            obj.convert(b);
            return b;
        }
        case msgpack::type::ARRAY: {
            nlohmann::json::array_t arr;
            for (size_t i = 0; i < obj.via.array.size; i++) {
                arr.push_back(convertMsgPackToJson(obj.via.array.ptr[i]));
            }
            return arr;
        }
        case msgpack::type::MAP: {
            nlohmann::json::object_t map;
            for (size_t i = 0; i < obj.via.map.size; i++) {
                const auto& pair = obj.via.map.ptr[i];
                std::string key;
                pair.key.convert(key);
                map[key] = convertMsgPackToJson(pair.val);
            }
            return map;
        }
        default:
            return nlohmann::json::object();
    }
 }

// MsgPack payload'ı map olarak parse eden yardımcı fonksiyon
inline nlohmann::json parseMsgPackPayload(const std::vector<uint8_t>& req) {
    try {
        // Önce msgpack'i parse et
        msgpack::object_handle oh = msgpack::unpack(
            reinterpret_cast<const char*>(req.data()),
            req.size()
        );
        
        // Debug için
        LOG_DEBUG("Raw payload size: " + std::to_string(req.size()));
        
        // Objeyi al
        msgpack::object obj = oh.get();
        
        // Recursive olarak objeyi JSON'a dönüştür
        return convertMsgPackToJson(obj);
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Parse error: " + std::string(ex.what()));
        return nlohmann::json::object();
    }
}
