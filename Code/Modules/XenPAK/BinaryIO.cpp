//
// Created by Jake Rieger on 9/6/2026.
//

#include "BinaryIO.hpp"
#include <Xen/Exception.hpp>

namespace Xen::PAK::BinaryIO {
    static void WriteBytes(u8* Bytes, const int Count, const u64 Value) {
        for (int i = 0; i < Count; ++i) {
            Bytes[i] = CAST<u8>((Value >> (i * 8)) & 0xFF);
        }
    }

    static u64 ReadBytes(std::istream& In, const int Count) {
        u8 Bytes[8] {};
        In.read(RCAST<char*>(Bytes), Count);
        if (!In) { _ThrowEngineException(EngineException, "ReadBytes: unexpected EOF"); }
        u64 Value = 0;
        for (int i = 0; i < Count; ++i) {
            Value |= CAST<u64>(Bytes[i]) << (i * 8);
        }
        return Value;
    }

    void WriteU16(std::ostream& Out, const u16 Value) {
        constexpr int Size = 2;
        u8 Bytes[Size];
        WriteBytes(Bytes, Size, Value);
        Out.write(RCAST<const char*>(Bytes), sizeof(Bytes));
    }

    void WriteU32(std::ostream& Out, const u32 Value) {
        constexpr int Size = 4;
        u8 Bytes[Size];
        WriteBytes(Bytes, Size, Value);
        Out.write(RCAST<const char*>(Bytes), sizeof(Bytes));
    }

    void WriteU64(std::ostream& Out, const u64 Value) {
        constexpr int Size = 8;
        u8 Bytes[Size];
        WriteBytes(Bytes, Size, Value);
        Out.write(RCAST<const char*>(Bytes), sizeof(Bytes));
    }

    u16 ReadU16(std::istream& In) {
        return CAST<u16>(ReadBytes(In, 2));
    }

    u32 ReadU32(std::istream& In) {
        return CAST<u32>(ReadBytes(In, 4));
    }

    u64 ReadU64(std::istream& In) {
        return ReadBytes(In, 8);
    }
}  // namespace Xen::PAK::BinaryIO