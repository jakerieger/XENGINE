//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetID.hpp"
#include "Aes.hpp"

#include <Common/Exception.hpp>
#include <array>
#include <vector>

namespace Xen::PAK {
    _DefineEngineException(CryptoException);

    constexpr size_t PAK_KEY_SIZE   = AES_256_KEY_SIZE;
    constexpr size_t PAK_SALT_SIZE  = 16;
    constexpr size_t PAK_NONCE_SIZE = AES_BLOCK_SIZE;

    constexpr size_t PAK_KEYCHECK_SIZE = AES_BLOCK_SIZE;

    using PakKey      = std::array<u8, PAK_KEY_SIZE>;
    using PakSalt     = std::array<u8, PAK_SALT_SIZE>;
    using PakNonce    = std::array<u8, PAK_NONCE_SIZE>;
    using PakKeyCheck = std::array<u8, PAK_KEYCHECK_SIZE>;

    /// @brief Generates a random salt for a new pak.
    PakSalt GenerateSalt();

    /// @brief Derives an asset's CTR nonce from its ID and the pak's salt.
    PakNonce DeriveNonce(AssetIDValue ID, const PakSalt& Salt, const AesKeySchedule& Schedule);

    /// @brief AES-256-CTR.
    void AesCtrXcrypt(const u8* Input, size_t Size, u8* Output, const AesKeySchedule& Schedule, const PakNonce& Nonce);

    /// @brief In-place convenience overload.
    void AesCtrXcryptInPlace(std::vector<u8>& Data, const AesKeySchedule& Schedule, const PakNonce& Nonce);

    /// @brief The build/runtime key.
    const PakKey& GetBuiltInKey();

    /// @brief Shared key schedule for the built-in key, expanded once.
    const AesKeySchedule& GetBuiltInKeySchedule();

    PakKeyCheck ComputeKeyCheck(const PakSalt& Salt, const AesKeySchedule& Schedule);
}  // namespace Xen::PAK