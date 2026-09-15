//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "PakFormat.hpp"

#include <Common/Exception.hpp>
#include <vector>
#include <memory>

namespace Xen::PAK {
    _DefineEngineException(CodecException);

    /// @brief Common interface for a single codec's compress/decompress pair.
    class ICodec {
    public:
        virtual ~ICodec() = default;

        virtual std::vector<u8> Compress(const u8* Data, size_t Size) = 0;
        virtual void
        Decompress(const u8* CompressedData, size_t CompressedSize, u8* OutData, size_t UncompressedSize) = 0;
        virtual PakCodec Codec() const                                                                    = 0;
    };

    std::unique_ptr<ICodec> CreateCodec(PakCodec Codec);
}  // namespace Xen::PAK