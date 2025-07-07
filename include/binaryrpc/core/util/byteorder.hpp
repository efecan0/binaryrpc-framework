#pragma once
#include <cstdint>
#ifdef _WIN32
#include <intrin.h>
#else
#include <endian.h>
#endif

namespace binaryrpc {
    /**
     * @brief Converts a 64-bit integer from host to network byte order (big endian).
     * @param value The value to convert.
     * @return The value in network byte order.
     */
    inline uint64_t hostToNetwork64(uint64_t value) {
#ifdef _WIN32
        return _byteswap_uint64(value);
#else
        return htobe64(value);
#endif
    }

    /**
     * @brief Converts a 64-bit integer from network byte order (big endian) to host byte order.
     * @param value The value to convert.
     * @return The value in host byte order.
     */
    inline uint64_t networkToHost64(uint64_t value) {
#ifdef _WIN32
        return _byteswap_uint64(value);
#else
        return be64toh(value);
#endif
    }
} 