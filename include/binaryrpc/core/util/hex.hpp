/**
 * @file hex.hpp
 * @brief Hexadecimal encoding and decoding utilities for BinaryRPC.
 *
 * Provides helper functions for converting between 16-byte binary buffers and 32-character hexadecimal strings.
 *
 * @author Efecan
 * @date 2025
 */
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace binaryrpc {

    /**
     * @brief Convert a 16-byte array to a 32-character lowercase hexadecimal string.
     *
     * @param buf 16-byte array to convert
     * @return 32-character lowercase hexadecimal string
     */
    inline std::string toHex(const std::array<uint8_t, 16>& buf)
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (auto b : buf)
            oss << std::setw(2) << static_cast<int>(b);
        return oss.str();
    }

    /**
     * @brief Convert a 32-character hexadecimal string to a 16-byte array.
     *
     * @param hex 32-character hexadecimal string
     * @param out Reference to a 16-byte array to fill
     * @throws std::invalid_argument if the input string is not 32 characters or contains invalid hex
     */
    inline void fromHex(std::string_view hex, std::array<uint8_t, 16>& out)
    {
        if (hex.size() != 32)
            throw std::invalid_argument("fromHex: length != 32");
        for (size_t i = 0; i < 16; ++i)
        {
            auto byte = std::stoul(std::string{ hex.substr(i * 2,2) }, nullptr, 16);
            out[i] = static_cast<uint8_t>(byte & 0xFF);
        }
    }

}