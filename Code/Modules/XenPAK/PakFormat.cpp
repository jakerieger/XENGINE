//
// Created by Jake Rieger on 9/7/2026.
//

#include "PakFormat.hpp"
#include "BinaryIO.hpp"

namespace Xen::PAK {
    using namespace BinaryIO;

    void PakHeader::Write(std::ostream& Out) const {
        Out.write(Magic.data(), Magic.size());
        WriteU32(Out, FormatVersion);
        WriteU64(Out, TableOffset);
        WriteU32(Out, TableEntryCount);
        Out.write(RCAST<const char*>(Salt.data()), CAST<std::streamsize>(Salt.size()));
        Out.write(RCAST<const char*>(KeyCheck.data()), CAST<std::streamsize>(KeyCheck.size()));
    }

    void PakHeader::WriteEmpty(std::ostream& Out) {
        PakHeader {}.Write(Out);
    }

    PakHeader PakHeader::Read(std::istream& In) {
        PakHeader Header;
        In.read(Header.Magic.data(), Header.Magic.size());
        if (!In || Header.Magic != PAK_MAGIC) {
            _ThrowEngineException(InvalidPakException, "bad magic (not a pak file or file is corrupt");
        }

        Header.FormatVersion = ReadU32(In);
        if (Header.FormatVersion != PAK_FORMAT_VERSION) {
            _ThrowEngineException(InvalidPakException,
                                  "unsupported format version: " + std::to_string(Header.FormatVersion));
        }

        Header.TableOffset     = ReadU64(In);
        Header.TableEntryCount = ReadU32(In);

        In.read(RCAST<char*>(Header.Salt.data()), CAST<std::streamsize>(Header.Salt.size()));
        if (!In) { _ThrowEngineException(InvalidPakException, "failed to read salt"); }

        In.read(RCAST<char*>(Header.KeyCheck.data()), CAST<std::streamsize>(Header.KeyCheck.size()));
        if (!In) { _ThrowEngineException(InvalidPakException, "failed to read key check"); }

        return Header;
    }

    //============================================================================================//

    void PakTableEntry::Write(std::ostream& Out) const {
        WriteU64(Out, ID);
        WriteU64(Out, Offset);
        WriteU32(Out, UncompressedSize);
        WriteU32(Out, CompressedSize);
        WriteU16(Out, CAST<u16>(Codec));
        WriteU16(Out, CAST<u16>(Flags));
    }

    PakTableEntry PakTableEntry::Read(std::istream& In) {
        PakTableEntry Entry;
        Entry.ID               = ReadU64(In);
        Entry.Offset           = ReadU64(In);
        Entry.UncompressedSize = ReadU32(In);
        Entry.CompressedSize   = ReadU32(In);
        Entry.Codec            = CAST<PakCodec>(ReadU16(In));
        Entry.Flags            = CAST<PakFlags>(ReadU16(In));

        return Entry;
    }
}  // namespace Xen::PAK