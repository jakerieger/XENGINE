
#include <PAK/AssetID.hpp>
#include <PAK/Codec.hpp>
#include <PAK/ContentScanner.hpp>
#include <PAK/PakFileSource.hpp>
#include <PAK/PakFormat.hpp>
#include <PAK/Crypto.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace Xen::PAK;

namespace {
    std::vector<u8> ReadWholeFile(const fs::path& Path) {
        std::ifstream File(Path, std::ios::binary | std::ios::ate);
        if (!File) {
            throw std::runtime_error(Pak_MakeExceptionStr("failed to open: " + Path.string()));
        }

        const std::streamsize Size = File.tellg();
        if (Size < 0) {
            throw std::runtime_error(Pak_MakeExceptionStr("failed to tellg: " + Path.string()));
        }
        File.seekg(0, std::ios::beg);

        std::vector<u8> Data(CAST<size_t>(Size));
        if (Size > 0 && !File.read(RCAST<char*>(Data.data()), Size)) {
            throw std::runtime_error(Pak_MakeExceptionStr("failed to read: " + Path.string()));
        }

        return Data;
    }

    void SelfVerify(const std::filesystem::path& PakPath,
                    const std::vector<ScannedAsset>& SourceAssets) {
        PakFileSource Source(PakPath, 0);

        if (Source.AssetCount() != SourceAssets.size()) {
            throw std::runtime_error(Pak_MakeExceptionStr("asset count mismatch"));
        }

        for (const auto& Asset : SourceAssets) {
            if (!Source.Contains(Asset.ID)) {
                throw std::runtime_error(
                  Pak_MakeExceptionStr("asset id not found in pak: " + Asset.CanonicalPath));
            }

            AssetBuffer Loaded             = Source.LoadFull(Asset.ID);
            const std::vector<u8> Original = ReadWholeFile(Asset.AbsolutePath);

            if (Loaded.Size() != Original.size()) {
                throw std::runtime_error(
                  Pak_MakeExceptionStr("size mismatch for " + Asset.CanonicalPath));
            }

            if (Loaded.Size() > 0 &&
                std::memcmp(Loaded.Data(), Original.data(), Loaded.Size()) != 0) {
                throw std::runtime_error(
                  Pak_MakeExceptionStr("byte mismatch for " + Asset.CanonicalPath));
            }
        }

        std::printf(
          "[XenPAK-CLI] self-verify OK: %llu assets, decompressed bytes match source exactly\n",
          SourceAssets.size());
    }
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <content_dir> <output.pak>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fs::path ContentDir = argv[1];
    fs::path OutputPath = argv[2];

    try {
        std::vector<ScannedAsset> Assets = ScanContentDirectory(ContentDir, CollisionPolicy::Throw);
        if (Assets.empty()) {
            std::fprintf(stderr, "[XenPAK-CLI] error: no assets found\n");
            return EXIT_FAILURE;
        }

        std::ofstream Out(OutputPath, std::ios::binary | std::ios::trunc);
        if (!Out) {
            std::fprintf(stderr,
                         "[XenPAK-CLI] error: failed to open output file '%s'\n",
                         OutputPath.string().c_str());
            return EXIT_FAILURE;
        }

        // Reserve space for the header now; the real table offset/entry count aren't known until
        // every asset has been written, so this gets overwritten in place later.
        PakHeader::WriteEmpty(Out);

        const PakSalt Salt                = GenerateSalt();
        const AesKeySchedule& KeySchedule = GetBuiltInKeySchedule();

        const std::unique_ptr<ICodec> Codec = CreateCodec(PakCodec::Lz4);

        std::vector<PakTableEntry> Table;
        Table.reserve(Assets.size());

        u64 TotalUncompressed = 0;
        u64 TotalStored       = 0;
        size_t StoredRawCount = 0;

        for (const auto& [ID, AbsolutePath, CanonicalPath] : Assets) {
            const std::vector<u8> Original = ReadWholeFile(AbsolutePath);

            /// Compress, but only keep results if actual compression is achieved (compressed size <
            /// original size).
            bool UseCompression = false;
            std::vector<u8> Compressed;
            if (!Original.empty()) {
                Compressed     = Codec->Compress(Original.data(), Original.size());
                UseCompression = Compressed.size() < Original.size();
            }

            const std::vector<u8>& Plain = UseCompression ? Compressed : Original;

            // Compress first, THEN encrypt.
            std::vector<u8> DataToWrite = Plain;
            AesCtrXcryptInPlace(DataToWrite, KeySchedule, DeriveNonce(ID.Value, Salt, KeySchedule));

            PakTableEntry Entry;
            Entry.ID               = ID.Value;
            Entry.Offset           = CAST<uint64_t>(Out.tellp());
            Entry.UncompressedSize = CAST<u32>(Original.size());
            Entry.CompressedSize   = CAST<u32>(DataToWrite.size());
            Entry.Codec            = UseCompression ? PakCodec::Lz4 : PakCodec::None;
            Entry.Flags            = PakFlags::None;

            if (!DataToWrite.empty()) {
                Out.write(RCAST<const char*>(DataToWrite.data()),
                          CAST<std::streamsize>(DataToWrite.size()));
            }
            Table.push_back(Entry);

            TotalUncompressed += Original.size();
            TotalStored += DataToWrite.size();
            if (!UseCompression) ++StoredRawCount;

            std::printf("[XenPAK-CLI] packed '%s' %zu -> %zu bytes (%s)\n",
                        CanonicalPath.c_str(),
                        Original.size(),
                        DataToWrite.size(),
                        UseCompression ? "lz4+aes" : "stored+aes");
        }

        u64 TableOffset = Out.tellp();
        for (const auto& Entry : Table) { Entry.Write(Out); }

        PakHeader Header;
        Header.TableOffset     = TableOffset;
        Header.TableEntryCount = CAST<u32>(Table.size());
        Header.Salt            = Salt;
        Header.KeyCheck        = ComputeKeyCheck(Salt, KeySchedule);
        Out.seekp(0, std::ios::beg);
        Header.Write(Out);

        Out.close();

        const f64 Ratio = TotalUncompressed > 0
                            ? 100.0 * (1.0 - CAST<f64>(TotalStored) / CAST<f64>(TotalUncompressed))
                            : 0.0;

        std::printf("[XenPAK-CLI] wrote '%s' (%zu assets, %zu stored raw, %llu -> %llu bytes, "
                    "%.1f%% smaller)\n",
                    OutputPath.string().c_str(),
                    Table.size(),
                    StoredRawCount,
                    CAST<u64>(TotalUncompressed),
                    CAST<u64>(TotalStored),
                    Ratio);

        SelfVerify(OutputPath, Assets);
    } catch (const std::exception& Ex) {
        std::fprintf(stderr, "[XenPAK-CLI] error: %s\n", Ex.what());
        return EXIT_FAILURE;
    }

    return 0;
}