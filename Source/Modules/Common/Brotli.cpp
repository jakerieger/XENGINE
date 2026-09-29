//
// Created by Jake Rieger on 9/29/2026.
//

#include "Brotli.hpp"

#include <brotli/encode.h>
#include <brotli/decode.h>

namespace Xen {
    std::optional<ByteArray> Brotli::Compress(const std::span<const uint8_t> Data) {
        const size_t MaxCompressedSize = BrotliEncoderMaxCompressedSize(Data.size_bytes());
        if (MaxCompressedSize == 0) {
            std::fprintf(stderr, "error: input data is too large for brotli compression\n");
            return {};
        }

        ByteArray Result(MaxCompressedSize);
        size_t EncodedSize = MaxCompressedSize;

        const BROTLI_BOOL Compressed = BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY,
                                                             BROTLI_DEFAULT_WINDOW,
                                                             BROTLI_DEFAULT_MODE,
                                                             Data.size_bytes(),
                                                             Data.data(),
                                                             &EncodedSize,
                                                             Result.data());
        if (!Compressed) {
            std::fprintf(stderr, "error: brotli compression failed\n");
            return {};
        }

        Result.resize(EncodedSize);
        return Result;
    }

    std::optional<ByteArray> Brotli::Decompress(const std::span<const uint8_t> Data, const size_t UncompressedSize) {
        BrotliDecoderState* State = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
        if (!State) {
            std::fprintf(stderr, "error: failed to create Brotli decoder state\n");
            return {};
        }

        ByteArray Result;
        if (UncompressedSize <= 0) {
            std::fprintf(stderr, "error: uncompressed size is zero\n");
            return {};
        }
        Result.resize(UncompressedSize);

        size_t AvailIn        = Data.size_bytes();
        const uint8_t* NextIn = Data.data();
        size_t AvailOut       = Result.size();
        uint8_t* NextOut      = Result.data();
        size_t TotalOut       = 0;

        for (;;) {
            const BrotliDecoderResult DecodeResult =
              BrotliDecoderDecompressStream(State, &AvailIn, &NextIn, &AvailOut, &NextOut, &TotalOut);

            if (DecodeResult == BROTLI_DECODER_RESULT_SUCCESS) break;
            if (DecodeResult == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
                const size_t CurrentPos = NextOut - Result.data();
                Result.resize(Result.size() * 2);
                NextOut  = Result.data() + CurrentPos;
                AvailOut = Result.size() - CurrentPos;
            } else {
                BrotliDecoderDestroyInstance(State);
                std::fprintf(stderr, "error: decompression failed\n");
                return {};
            }
        }

        Result.resize(TotalOut);
        BrotliDecoderDestroyInstance(State);

        return Result;
    }
}  // namespace Xen