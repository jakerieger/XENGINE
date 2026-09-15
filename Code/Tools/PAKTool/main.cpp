#include <XenPAK/AssetID.hpp>
#include <XenPAK/Codec.hpp>
#include <XenPAK/ContentScanner.hpp>
#include <XenPAK/PakFileSource.hpp>
#include <XenPAK/PakFormat.hpp>
#include <XenPAK/PakManifest.hpp>
#include <XenPAK/Crypto.hpp>

#include <CLI/CLI.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

namespace fs = std::filesystem;
using namespace Xen::PAK;

namespace {
    std::vector<u8> ReadWholeFile(const fs::path& Path) {
        std::ifstream File(Path, std::ios::binary | std::ios::ate);
        if (!File) { throw std::runtime_error(Pak_MakeExceptionStr("failed to open: " + Path.string())); }

        const std::streamsize Size = File.tellg();
        if (Size < 0) { throw std::runtime_error(Pak_MakeExceptionStr("failed to tellg: " + Path.string())); }
        File.seekg(0, std::ios::beg);

        std::vector<u8> Data(CAST<size_t>(Size));
        if (Size > 0 && !File.read(RCAST<char*>(Data.data()), Size)) {
            throw std::runtime_error(Pak_MakeExceptionStr("failed to read: " + Path.string()));
        }

        return Data;
    }

    const char* CodecName(const PakCodec Codec) {
        switch (Codec) {
            case PakCodec::None:
                return "None";
            case PakCodec::ZStd:
                return "ZStd";
            case PakCodec::Lz4:
                return "Lz4";
        }
        return "Unknown";
    }

    void SelfVerify(const fs::path& PakPath, const std::vector<ScannedAsset>& SourceAssets) {
        PakFileSource Source(PakPath, 0);

        if (Source.AssetCount() != SourceAssets.size()) {
            throw std::runtime_error(Pak_MakeExceptionStr("asset count mismatch"));
        }

        for (const auto& Asset : SourceAssets) {
            if (!Source.Contains(Asset.ID)) {
                throw std::runtime_error(Pak_MakeExceptionStr("asset id not found in pak: " + Asset.CanonicalPath));
            }

            AssetBuffer Loaded             = Source.LoadFull(Asset.ID);
            const std::vector<u8> Original = ReadWholeFile(Asset.AbsolutePath);

            if (Loaded.Size() != Original.size()) {
                throw std::runtime_error(Pak_MakeExceptionStr("size mismatch for " + Asset.CanonicalPath));
            }

            if (Loaded.Size() > 0 && std::memcmp(Loaded.Data(), Original.data(), Loaded.Size()) != 0) {
                throw std::runtime_error(Pak_MakeExceptionStr("byte mismatch for " + Asset.CanonicalPath));
            }
        }

        std::printf("[PAKTool] self-verify OK: %llu assets, decompressed bytes match source exactly\n",
                    CAST<u64>(SourceAssets.size()));
    }

    int RunPack(const fs::path& ContentDir, const fs::path& OutputPath) {
        std::vector<ScannedAsset> Assets = ScanContentDirectory(ContentDir, CollisionPolicy::Throw);
        if (Assets.empty()) {
            std::fprintf(stderr, "[PAKTool] error: no assets found\n");
            return EXIT_FAILURE;
        }

        std::ofstream Out(OutputPath, std::ios::binary | std::ios::trunc);
        if (!Out) {
            std::fprintf(stderr, "[PAKTool] error: failed to open output file '%s'\n", OutputPath.string().c_str());
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

        PakManifest Manifest;
        Manifest.PakFilename = OutputPath.filename().string();
        Manifest.Assets.reserve(Assets.size());

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
                Out.write(RCAST<const char*>(DataToWrite.data()), CAST<std::streamsize>(DataToWrite.size()));
            }
            Table.push_back(Entry);

            PakManifestEntry ManifestEntry;
            ManifestEntry.ID               = ID.Value;
            ManifestEntry.CanonicalPath    = CanonicalPath;
            ManifestEntry.UncompressedSize = Entry.UncompressedSize;
            ManifestEntry.CompressedSize   = Entry.CompressedSize;
            ManifestEntry.Codec            = Entry.Codec;
            ManifestEntry.Compressed       = UseCompression;
            ManifestEntry.Encrypted        = true;  // Every asset is AES-256-CTR encrypted unconditionally.
            Manifest.Assets.push_back(std::move(ManifestEntry));

            TotalUncompressed += Original.size();
            TotalStored += DataToWrite.size();
            if (!UseCompression) ++StoredRawCount;

            std::printf("[PAKTool] packed '%s' %zu -> %zu bytes (%s)\n",
                        CanonicalPath.c_str(),
                        Original.size(),
                        DataToWrite.size(),
                        UseCompression ? "lz4+aes" : "stored+aes");
        }

        u64 TableOffset = Out.tellp();
        for (const auto& Entry : Table) {
            Entry.Write(Out);
        }

        PakHeader Header;
        Header.TableOffset     = TableOffset;
        Header.TableEntryCount = CAST<u32>(Table.size());
        Header.Salt            = Salt;
        Header.KeyCheck        = ComputeKeyCheck(Salt, KeySchedule);
        Out.seekp(0, std::ios::beg);
        Header.Write(Out);

        Out.close();

        const fs::path ManifestPath = PakManifest::ManifestPathFor(OutputPath);
        Manifest.WriteToFile(ManifestPath);

        const f64 Ratio =
          TotalUncompressed > 0 ? 100.0 * (1.0 - CAST<f64>(TotalStored) / CAST<f64>(TotalUncompressed)) : 0.0;

        std::printf("[PAKTool] wrote '%s' (%zu assets, %zu stored raw, %llu -> %llu bytes, "
                    "%.1f%% smaller)\n",
                    OutputPath.string().c_str(),
                    Table.size(),
                    StoredRawCount,
                    CAST<u64>(TotalUncompressed),
                    CAST<u64>(TotalStored),
                    Ratio);
        std::printf("[PAKTool] wrote manifest '%s'\n", ManifestPath.string().c_str());

        SelfVerify(OutputPath, Assets);

        return EXIT_SUCCESS;
    }

    int RunUnpack(const fs::path& PakPath, const fs::path& OutputDir) {
        const fs::path ManifestPath = PakManifest::ManifestPathFor(PakPath);
        if (!fs::exists(ManifestPath)) {
            std::fprintf(stderr,
                         "[PAKTool] error: manifest not found at '%s' (required to unpack - "
                         "re-run 'pack' to regenerate it)\n",
                         ManifestPath.string().c_str());
            return EXIT_FAILURE;
        }
        const PakManifest Manifest = PakManifest::ReadFromFile(ManifestPath);

        PakFileSource Source(PakPath, 0);
        fs::create_directories(OutputDir);

        size_t Count = 0;
        for (const auto& Asset : Manifest.Assets) {
            const AssetID ID(Asset.ID);
            if (!Source.Contains(ID)) {
                throw std::runtime_error(
                  Pak_MakeExceptionStr("manifest references asset not present in pak: " + Asset.CanonicalPath));
            }

            AssetBuffer Buffer = Source.LoadFull(ID);

            const fs::path OutPath = OutputDir / fs::path(Asset.CanonicalPath);
            fs::create_directories(OutPath.parent_path());

            std::ofstream OutFile(OutPath, std::ios::binary | std::ios::trunc);
            if (!OutFile) {
                throw std::runtime_error(Pak_MakeExceptionStr("failed to open output file: " + OutPath.string()));
            }
            if (Buffer.Size() > 0) {
                OutFile.write(RCAST<const char*>(Buffer.Data()), CAST<std::streamsize>(Buffer.Size()));
            }

            std::printf("[PAKTool] unpacked '%s' (%zu bytes)\n", Asset.CanonicalPath.c_str(), Buffer.Size());
            ++Count;
        }

        std::printf("[PAKTool] unpacked %zu assets to '%s'\n", Count, OutputDir.string().c_str());

        return EXIT_SUCCESS;
    }

    int RunInfo(const fs::path& PakPath) {
        std::ifstream In(PakPath, std::ios::binary);
        if (!In) {
            std::fprintf(stderr, "[PAKTool] error: failed to open pak file '%s'\n", PakPath.string().c_str());
            return EXIT_FAILURE;
        }
        const PakHeader Header = PakHeader::Read(In);
        In.close();

        const fs::path ManifestPath = PakManifest::ManifestPathFor(PakPath);
        if (!fs::exists(ManifestPath)) {
            std::fprintf(stderr,
                         "[PAKTool] error: manifest not found at '%s' (required for 'info' - "
                         "re-run 'pack' to regenerate it)\n",
                         ManifestPath.string().c_str());
            return EXIT_FAILURE;
        }
        const PakManifest Manifest = PakManifest::ReadFromFile(ManifestPath);

        u64 TotalUncompressed  = 0;
        u64 TotalCompressed    = 0;
        size_t CompressedCount = 0;
        size_t EncryptedCount  = 0;
        for (const auto& Asset : Manifest.Assets) {
            TotalUncompressed += Asset.UncompressedSize;
            TotalCompressed += Asset.CompressedSize;
            if (Asset.Compressed) ++CompressedCount;
            if (Asset.Encrypted) ++EncryptedCount;
        }

        std::printf("Pak file:       %s\n", PakPath.string().c_str());
        std::printf("Manifest file:  %s\n", ManifestPath.string().c_str());
        std::printf("Format version: %u\n", Header.FormatVersion);
        std::printf("Asset count:    %u\n", Header.TableEntryCount);
        if (Manifest.Assets.size() != Header.TableEntryCount) {
            std::printf("  warning: manifest has %zu assets but pak table has %u entries\n",
                        Manifest.Assets.size(),
                        Header.TableEntryCount);
        }
        std::printf("Total size:     %llu -> %llu bytes\n", CAST<u64>(TotalUncompressed), CAST<u64>(TotalCompressed));
        std::printf("Compressed:     %zu / %zu assets\n", CompressedCount, Manifest.Assets.size());
        std::printf("Encrypted:      %zu / %zu assets\n", EncryptedCount, Manifest.Assets.size());
        std::printf("\n");

        std::vector<PakManifestEntry> Sorted = Manifest.Assets;
        std::ranges::sort(Sorted, [](const PakManifestEntry& A, const PakManifestEntry& B) {
            return A.CanonicalPath < B.CanonicalPath;
        });

        std::printf("%-16s  %-6s  %-10s  %-10s  %11s  %11s  %s\n",
                    "id",
                    "codec",
                    "compressed",
                    "encrypted",
                    "raw bytes",
                    "stored bytes",
                    "path");
        for (const auto& Asset : Sorted) {
            std::printf("%016llx  %-6s  %-10s  %-10s  %11u  %11u  %s\n",
                        CAST<u64>(Asset.ID),
                        CodecName(Asset.Codec),
                        Asset.Compressed ? "yes" : "no",
                        Asset.Encrypted ? "yes" : "no",
                        Asset.UncompressedSize,
                        Asset.CompressedSize,
                        Asset.CanonicalPath.c_str());
        }

        return EXIT_SUCCESS;
    }
}  // namespace

int main(int argc, char** argv) {
    CLI::App App {"PAKTool — asset packing tool for Xen"};
    App.require_subcommand(1);

    fs::path PackContentDir;
    fs::path PackOutput = "Data1.xpak";
    CLI::App* PackCmd   = App.add_subcommand("pack", "Pack a content directory into a .xpak file");
    PackCmd->add_option("content-dir", PackContentDir, "Root content directory to pack")
      ->required()
      ->check(CLI::ExistingDirectory);
    PackCmd->add_option("-o,--output",
                        PackOutput,
                        "Output .xpak filename (default: Data1.xpak). Must use .xpak extension.");

    fs::path UnpackPak;
    fs::path UnpackOutputDir;
    CLI::App* UnpackCmd = App.add_subcommand("unpack", "Unpack all assets from a .xpak file");
    UnpackCmd->add_option("pak-file", UnpackPak, "Pak file to unpack")->required()->check(CLI::ExistingFile);
    UnpackCmd->add_option("output-dir", UnpackOutputDir, "Directory to write unpacked assets to")->required();

    fs::path InfoPak;
    CLI::App* InfoCmd = App.add_subcommand("info", "Print detailed information about a .xpak file");
    InfoCmd->add_option("pak-file", InfoPak, "Pak file to inspect")->required()->check(CLI::ExistingFile);

    CLI11_PARSE(App, argc, argv);

    try {
        if (*PackCmd) return RunPack(PackContentDir, PackOutput);
        if (*UnpackCmd) return RunUnpack(UnpackPak, UnpackOutputDir);
        if (*InfoCmd) return RunInfo(InfoPak);
    } catch (const std::exception& Ex) {
        std::fprintf(stderr, "[PAKTool] error: %s\n", Ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
