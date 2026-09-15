//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include <Xen/EngineCommon.hpp>

#include <iostream>

namespace Xen::PAK::BinaryIO {
    void WriteU16(std::ostream& Out, u16 Value);
    void WriteU32(std::ostream& Out, u32 Value);
    void WriteU64(std::ostream& Out, u64 Value);

    u16 ReadU16(std::istream& In);
    u32 ReadU32(std::istream& In);
    u64 ReadU64(std::istream& In);
}  // namespace Xen::PAK::BinaryIO