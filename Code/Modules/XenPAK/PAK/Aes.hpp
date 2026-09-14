//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "PakCommon.hpp"

#include <array>

namespace Xen::PAK {
    constexpr size_t AES_BLOCK_SIZE   = 16;
    constexpr size_t AES_256_KEY_SIZE = 32;
    constexpr size_t AES_256_ROUNDS   = 14;

    /// @brief Expanded AES-256 round keys. Built once per key and reused for
    /// every block, so the key schedule isn't recomputed per call.
    class AesKeySchedule {
    public:
        explicit AesKeySchedule(const std::array<u8, AES_256_KEY_SIZE>& Key);

        /// @brief Encrypts exactly one 16-byte block in place.
        void EncryptBlock(u8 Block[AES_BLOCK_SIZE]) const;

    private:
        // 4 * (Rounds + 1) words of 4 bytes each.
        std::array<u8, 4 * 4 * (AES_256_ROUNDS + 1)> _RoundKeys {};
    };
}  // namespace Xen::PAK