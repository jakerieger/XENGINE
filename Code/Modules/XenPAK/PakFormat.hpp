//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetID.hpp"
#include "Crypto.hpp"

#include <Xen/Exception.hpp>
#include <array>

namespace Xen::PAK {
    _DefineEngineException(InvalidPakException);

    constexpr std::array PAK_MAGIC   = {'X', 'P', 'A', 'K'};
    constexpr u32 PAK_FORMAT_VERSION = 2;  // v2 added encryption salt

    enum class PakCodec : u16 {
        None = 0,
        ZStd = 1,
        Lz4  = 2,
    };

    enum class PakFlags : u16 {
        None       = 0,
        Streamable = 1 << 0,
    };

    struct PakHeader {
        std::array<char, 4> Magic = PAK_MAGIC;
        u32 FormatVersion         = PAK_FORMAT_VERSION;
        u64 TableOffset           = 0;
        u32 TableEntryCount       = 0;

        /// @brief Per-pak random salt used to derive each asset's CTR none.
        PakSalt Salt {};

        /// @brief Known constant encrypted under the key. Used for detecting invalid key at
        /// read-time.
        PakKeyCheck KeyCheck {};

        void Write(std::ostream& Out) const;
        static void WriteEmpty(std::ostream& Out);

        /// @brief Attempts to read a pak file header.
        /// @throws InvalidPakException On bad magic or unsupported version
        static PakHeader Read(std::istream& In);
    };

    struct PakTableEntry {
        AssetIDValue ID      = 0;
        u64 Offset           = 0;
        u32 UncompressedSize = 0;
        u32 CompressedSize   = 0;
        PakCodec Codec       = PakCodec::None;
        PakFlags Flags       = PakFlags::None;

        void Write(std::ostream& Out) const;
        static PakTableEntry Read(std::istream& In);
    };
}  // namespace Xen::PAK