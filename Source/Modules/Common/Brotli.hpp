//
// Created by Jake Rieger on 9/29/2026.
//

#pragma once

#include "XenCommon.hpp"

#include <span>

namespace Xen::Brotli {
    std::optional<ByteArray> Compress(std::span<const uint8_t> Data);
    std::optional<ByteArray> Decompress(std::span<const uint8_t> Data, size_t UncompressedSize);
}  // namespace Xen::Brotli
