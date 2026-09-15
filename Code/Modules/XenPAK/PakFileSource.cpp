//
// Created by Jake Rieger on 9/7/2026.
//

#include "PakFileSource.hpp"
#include "Codec.hpp"

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

namespace Xen::PAK {
    PakFileSource::PakFileSource(std::filesystem::path PakPath, int Priority)
        : _PakPath(std::move(PakPath)), _Priority(Priority) {
        OpenAndReadTable();
    }

    void PakFileSource::OpenAndReadTable() {
        _FileStream.open(_PakPath, std::ios::binary);
        if (!_FileStream) { THROW_ENGINE_EXCEPTION(EngineException, "failed to open pak file: " + _PakPath.string()); }

        const PakHeader Header = PakHeader::Read(_FileStream);

        _FileStream.seekg(CAST<std::streamoff>(Header.TableOffset), std::ios::beg);
        if (!_FileStream) {
            THROW_ENGINE_EXCEPTION(EngineException, "failed to seek to table offset in: " + _PakPath.string());
        }

        _Salt = Header.Salt;

        if (ComputeKeyCheck(_Salt, GetBuiltInKeySchedule()) != Header.KeyCheck) {
            THROW_ENGINE_EXCEPTION(CryptoException, "key check failed - wrong decryption key for: " + _PakPath.string());
        }

        _Table.reserve(Header.TableEntryCount);
        for (u32 i = 0; i < Header.TableEntryCount; ++i) {
            PakTableEntry Entry = PakTableEntry::Read(_FileStream);
            _Table[Entry.ID]    = Entry;
        }
    }

    bool PakFileSource::Contains(const AssetID ID) const {
        return _Table.contains(ID.Value);
    }

    AssetBuffer PakFileSource::LoadFull(const AssetID ID) {
        const auto It = _Table.find(ID.Value);
        if (It == _Table.end()) {
            THROW_ENGINE_EXCEPTION(EngineException, "unknown asset ID: " + std::to_string(ID.Value));
        }

        const PakTableEntry& Entry = It->second;

        _FileStream.clear();
        _FileStream.seekg(CAST<std::streamoff>(Entry.Offset), std::ios::beg);
        if (!_FileStream) {
            THROW_ENGINE_EXCEPTION(EngineException, "seek failed for asset ID " + std::to_string(ID.Value));
        }

        std::vector<u8> Stored(Entry.CompressedSize);
        if (Entry.CompressedSize > 0 && !_FileStream.read(RCAST<char*>(Stored.data()), Entry.CompressedSize)) {
            THROW_ENGINE_EXCEPTION(EngineException, "read failed for asset ID " + std::to_string(ID.Value));
        }

        // Compress and then encrypt (decrypt and then decompress)
        AesCtrXcryptInPlace(Stored, GetBuiltInKeySchedule(), DeriveNonce(Entry.ID, _Salt, GetBuiltInKeySchedule()));

        auto Data = std::make_unique<u8[]>(Entry.UncompressedSize);

        if (Entry.Codec == PakCodec::None) {
            if (Entry.UncompressedSize > 0) { std::memcpy(Data.get(), Stored.data(), Entry.UncompressedSize); }
        } else {
            const std::unique_ptr<ICodec> Codec = CreateCodec(Entry.Codec);
            if (!Codec) { THROW_ENGINE_EXCEPTION(EngineException, "no compressor for codec"); }

            Codec->Decompress(Stored.data(), Entry.CompressedSize, Data.get(), Entry.UncompressedSize);
        }

        return AssetBuffer(std::move(Data), Entry.UncompressedSize);
    }

    int PakFileSource::Priority() const {
        return _Priority;
    }
}  // namespace Xen::PAK