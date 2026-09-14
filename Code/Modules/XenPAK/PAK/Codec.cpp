//
// Created by Jake Rieger on 9/7/2026.
//

#include "Codec.hpp"

#include <lz4.h>
#include <stdexcept>
#include <limits>

namespace Xen::PAK {
    namespace {
        class Lz4Codec final : public ICodec {
        public:
            std::vector<u8> Compress(const u8* Data, const size_t Size) override {
                if (Size > CAST<size_t>(LZ4_MAX_INPUT_SIZE)) {
                    throw CodecException(Pak_MakeExceptionStr("Input exceeds LZ4_MAX_INPUT_SIZE"));
                }

                const int SrcSize = CAST<int>(Size);
                const int Bound   = LZ4_compressBound(SrcSize);
                if (Bound <= 0) {
                    throw CodecException(Pak_MakeExceptionStr("LZ4_compressBound failed"));
                }

                std::vector<u8> Out(CAST<size_t>(Bound));
                const int Result = LZ4_compress_default(RCAST<const char*>(Data),
                                                        RCAST<char*>(Out.data()),
                                                        SrcSize,
                                                        Bound);
                if (Result <= 0) {
                    throw CodecException(Pak_MakeExceptionStr("LZ4_compress_default failed"));
                }

                Out.resize(CAST<size_t>(Result));
                return Out;
            }

            void Decompress(const u8* CompressedData,
                            const size_t CompressedSize,
                            u8* OutData,
                            const size_t UncompressedSize) override {
                if (CompressedSize > CAST<size_t>(std::numeric_limits<int>::max()) ||
                    UncompressedSize > CAST<size_t>(std::numeric_limits<int>::max())) {
                    throw CodecException(Pak_MakeExceptionStr("size exceeds int range"));
                }

                const int Result = LZ4_decompress_safe(RCAST<const char*>(CompressedData),
                                                       RCAST<char*>(OutData),
                                                       CAST<int>(CompressedSize),
                                                       CAST<int>(UncompressedSize));

                if (Result < 0) {
                    throw CodecException(
                      Pak_MakeExceptionStr("LZ4_decompress_safe failed (possibly corrupt pak)"));
                }

                if (CAST<size_t>(Result) != UncompressedSize) {
                    throw CodecException(
                      Pak_MakeExceptionStr("decompressed size does not match expected size"));
                }
            }

            PakCodec Codec() const override { return PakCodec::Lz4; }
        };
    }  // namespace

    std::unique_ptr<ICodec> CreateCodec(const PakCodec Codec) {
        switch (Codec) {
            case PakCodec::None: return nullptr;
            case PakCodec::Lz4: return std::make_unique<Lz4Codec>();
            case PakCodec::ZStd:
                throw std::runtime_error("CreateCodec - ZStd codec not supported yet");
        }

        throw std::runtime_error("CreateCodec - Unknown codec");
    }
}  // namespace Xen::PAK