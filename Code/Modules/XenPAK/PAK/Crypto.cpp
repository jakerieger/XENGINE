
//
// Created by Jake Rieger on 9/7/2026.
//

#include "Crypto.hpp"

#include <random>
#include <cstring>

namespace Xen::PAK {
    PakSalt GenerateSalt() {
        std::random_device Rd;
        std::mt19937_64 Gen(((CAST<u64>(Rd()) << 32) ^ Rd()));
        std::uniform_int_distribution<unsigned int> Dist(0, 255);

        PakSalt Salt {};
        for (auto& B : Salt) { B = CAST<u8>(Dist(Gen)); }
        return Salt;
    }

    PakNonce
    DeriveNonce(const AssetIDValue ID, const PakSalt& Salt, const AesKeySchedule& Schedule) {
        PakNonce Nonce {};
        std::memcpy(Nonce.data(), Salt.data(), PAK_SALT_SIZE);

        // XOR the ID into the low 8 bytes, serialized little-endian
        // explicitly so derivation is identical regardless of host
        // endianness - a pak built on one machine must decrypt on any other.
        for (size_t i = 0; i < sizeof(AssetIDValue); ++i) {
            Nonce[i] ^= CAST<u8>((ID >> (i * 8)) & 0xFF);
        }

        // One AES pass diffuses the result, so nonces for sequential or
        // similar asset IDs are not related in any usable way.
        Schedule.EncryptBlock(Nonce.data());
        return Nonce;
    }

    void AesCtrXcrypt(const u8* Input,
                      const size_t Size,
                      u8* Output,
                      const AesKeySchedule& Schedule,
                      const PakNonce& Nonce) {
        if (Size == 0) return;

        u8 Counter[AES_BLOCK_SIZE];
        std::memcpy(Counter, Nonce.data(), AES_BLOCK_SIZE);

        u8 Keystream[AES_BLOCK_SIZE];
        size_t Processed = 0;

        while (Processed < Size) {
            std::memcpy(Keystream, Counter, AES_BLOCK_SIZE);
            Schedule.EncryptBlock(Keystream);

            const size_t ChunkSize = std::min(CAST<size_t>(AES_BLOCK_SIZE), Size - Processed);
            for (size_t i = 0; i < ChunkSize; ++i) {
                Output[Processed + i] = CAST<u8>(Input[Processed + i] ^ Keystream[i]);
            }
            Processed += ChunkSize;

            // Increment the 128-bit counter as a big-endian integer, matching
            // the standard CTR construction (NIST SP 800-38A).
            for (int i = AES_BLOCK_SIZE - 1; i >= 0; --i) {
                if (++Counter[i] != 0) break;
            }
        }
    }

    void AesCtrXcryptInPlace(std::vector<u8>& Data,
                             const AesKeySchedule& Schedule,
                             const PakNonce& Nonce) {
        if (Data.empty()) return;
        AesCtrXcrypt(Data.data(), Data.size(), Data.data(), Schedule, Nonce);
    }

    const PakKey& GetBuiltInKey() {
        // PLACEHOLDER KEY - see the warning in Crypto.hpp. This is a
        // contiguous, trivially-greppable array in .rodata, the weakest
        // possible form. Replace with a per-project generated key, and later
        // with runtime-derived material, as a hardening pass.
        static const PakKey Key = {0x8F, 0x2A, 0xC1, 0x77, 0x3E, 0xB9, 0x04, 0xD6, 0x51, 0xEE, 0x1B,
                                   0xA3, 0x9C, 0x60, 0x2F, 0xD8, 0x47, 0x13, 0xBC, 0x85, 0x6A, 0xF0,
                                   0x29, 0x5D, 0xE4, 0x38, 0x71, 0xCA, 0x0B, 0x96, 0xAF, 0x52};
        return Key;
    }

    PakKeyCheck ComputeKeyCheck(const PakSalt& Salt, const AesKeySchedule& Schedule) {
        // A fixed, arbitrary constant. Its value doesn't matter - only that
        // the packer and reader agree on it.
        static constexpr u8 KEYCHECK_MAGIC[AES_BLOCK_SIZE] =
          {'X', 'e', 'n', 'P', 'A', 'K', '-', 'K', 'e', 'y', 'C', 'h', 'e', 'c', 'k', '!'};

        PakKeyCheck Check {};
        std::memcpy(Check.data(), KEYCHECK_MAGIC, AES_BLOCK_SIZE);

        // Mix in the salt so the stored value differs per pak; otherwise
        // every pak built with the same key would carry an identical,
        // recognizable 16-byte fingerprint in its header.
        for (size_t i = 0; i < PAK_SALT_SIZE && i < AES_BLOCK_SIZE; ++i) { Check[i] ^= Salt[i]; }

        Schedule.EncryptBlock(Check.data());
        return Check;
    }

    const AesKeySchedule& GetBuiltInKeySchedule() {
        // Expanded once on first use; the schedule is read-only afterward.
        static const AesKeySchedule Schedule(GetBuiltInKey());
        return Schedule;
    }
}  // namespace Xen::PAK
