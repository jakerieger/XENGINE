//
// Created by Jake Rieger on 9/19/2026.
//
// A streaming Radiance (.hdr, RGBE) decoder for environment maps.
//
// stb_image's HDR loader decodes the whole image to 32-bit float first - an 8K
// equirect capture is half a gigabyte of floats before a single texel has been
// downsampled or converted, plus copies for the mip pyramid. This decodes one
// scanline at a time and folds each straight into a box-downsampled RGBA16F
// image, so peak memory is the (capped) output plus a scanline, no matter how
// large the source is.

#pragma once

#include <Common/XenCommon.hpp>

#include <string>
#include <vector>

namespace Xen {
    struct RadianceImage {
        u32 Width {0};   // after any downsampling to MaxWidth
        u32 Height {0};
        u32 SourceWidth {0};   // as stored in the file
        u32 SourceHeight {0};

        /// Mip 0, tightly packed RGBA16F (8 bytes per texel).
        std::vector<u8> Mip0;

        /// Mips 1..N, each RGBA16F: a solid-angle-weighted 2x2 box filter of
        /// the level before it, down to 1x1. Solid-angle weighting because
        /// this is an equirectangular map - a row near a pole covers far less
        /// of the sphere than one at the equator, so it must count for less
        /// when two rows are averaged.
        std::vector<std::vector<u8>> MipTail;
    };

    /// @brief Whether Bytes starts with a Radiance file signature.
    NODISCARD bool IsRadianceHdr(const u8* Bytes, size_t Size);

    /// @brief Decodes a Radiance .hdr (flat or run-length-encoded scanlines,
    /// "-Y H +X W" orientation only) to RGBA16F plus its mip pyramid.
    ///
    /// If the image is wider than MaxWidth it is box-downsampled by the
    /// smallest power of two that brings it within MaxWidth, while decoding.
    /// Values are clamped to half-float range so an over-bright sun disc can't
    /// become infinity. Returns false with Error set on a malformed file.
    bool DecodeRadianceHdr(const u8* Bytes, size_t Size, u32 MaxWidth, RadianceImage& Out, std::string& Error);
}  // namespace Xen
