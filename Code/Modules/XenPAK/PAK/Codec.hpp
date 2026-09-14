//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "PakCommon.hpp"
#include "PakFormat.hpp"

#include <vector>
#include <memory>

namespace Xen::PAK {
    Pak_MakeException(CodecException);

    /// @brief Common intetrface for a single codec's compress/decompress pair.
    class ICodec {
    public:
        virtual ~ICodec() = default;

        virtual std::vector<u8> Compress(const u8* Data, size_t Size) = 0;
        virtual void Decompress(const u8* CompressedData,
                                size_t CompressedSize,
                                u8* OutData,
                                size_t UncompressedSize)              = 0;
        virtual PakCodec Codec() const                                = 0;
    };

    std::unique_ptr<ICodec> CreateCodec(PakCodec Codec);
}  // namespace Xen::PAK