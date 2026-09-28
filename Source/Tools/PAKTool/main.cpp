#include <Common/Log.hpp>

#include <XenPAK/AssetID.hpp>
#include <XenPAK/Canonicalize.hpp>
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
#include <algorithm>

namespace fs = std::filesystem;
using namespace Xen;
using namespace Xen::PAK;

namespace {
    std::vector<u8> ReadWholeFile(const fs::path& Path) {
        std::ifstream File(Path, std::ios::binary | std::ios::ate);
        if (!File) { THROW_ENGINE_EXCEPTION(EngineException, "failed to open: " + Path.string()); }

        const std::streamsize Size = File.tellg();
        if (Size < 0) { THROW_ENGINE_EXCEPTION(EngineException, "failed to tellg: " + Path.string()); }
        File.seekg(0, std::ios::beg);

        std::vector<u8> Data(CAST<size_t>(Size));
        if (Size > 0 && !File.read(RCAST<char*>(Data.data()), Size)) {
            THROW_ENGINE_EXCEPTION(EngineException, "failed to read: " + Path.string());
        }

        return Data;
    }

    /// @brief Matches Text against Pattern, where '*' matches any run of
    /// characters (including none, and including '/' - patterns here aren't
    /// segment-aware the way a real .gitignore's '*' is, so "temp/*" already
    /// matches everything under temp/ at any depth; there's no separate '**'
    /// syntax) and '?' matches exactly one character. Classic two-pointer
    /// greedy wildcard match - backtracks to the most recent '*' on a
    /// mismatch rather than exploring every split, which is what keeps it
    /// linear instead of exponential.
    bool GlobMatch(const std::string_view Text, const std::string_view Pattern) {
        size_t TextIdx = 0, PatternIdx = 0;
        size_t StarIdx = std::string_view::npos, MatchIdx = 0;

        while (TextIdx < Text.size()) {
            if (PatternIdx < Pattern.size() && (Pattern[PatternIdx] == '?' || Pattern[PatternIdx] == Text[TextIdx])) {
                ++TextIdx;
                ++PatternIdx;
            } else if (PatternIdx < Pattern.size() && Pattern[PatternIdx] == '*') {
                StarIdx  = PatternIdx;
                MatchIdx = TextIdx;
                ++PatternIdx;
            } else if (StarIdx != std::string_view::npos) {
                PatternIdx = StarIdx + 1;
                MatchIdx += 1;
                TextIdx = MatchIdx;
            } else {
                return false;
            }
        }

        while (PatternIdx < Pattern.size() && Pattern[PatternIdx] == '*')
            ++PatternIdx;

        return PatternIdx == Pattern.size();
    }

    /// @brief One parsed line from a .pakignore file.
    struct IgnorePattern {
        std::string Text;        ///< Canonicalized (lowercase, '/'-separated) pattern text.
        std::string Raw;         ///< Original line, for diagnostics/printing.
        bool MatchBasenameOnly;  ///< True if the raw pattern had no '/' - matches the filename at any depth.
    };

    /// @brief Parses raw .pakignore lines into IgnorePatterns: blank lines
    /// and lines starting with '#' are skipped, trailing '\r' is trimmed (a
    /// .pakignore authored on Windows and read with std::getline(..., '\n')
    /// would otherwise leave one on every line), and each pattern is run
    /// through the same Canonicalize() used for asset paths so casing and
    /// slash direction can't cause a pattern to silently fail to match.
    std::vector<IgnorePattern> ParseIgnorePatterns(const std::vector<std::string>& RawLines) {
        std::vector<IgnorePattern> Patterns;
        Patterns.reserve(RawLines.size());

        for (const std::string& RawLine : RawLines) {
            std::string Line = RawLine;
            while (!Line.empty() && (Line.back() == '\r' || Line.back() == ' ' || Line.back() == '\t')) {
                Line.pop_back();
            }
            size_t FirstNonSpace = Line.find_first_not_of(" \t");
            if (FirstNonSpace == std::string::npos || Line[FirstNonSpace] == '#') continue;
            if (FirstNonSpace > 0) Line.erase(0, FirstNonSpace);

            const bool HasSlash = Line.find('/') != std::string::npos;

            Patterns.push_back(IgnorePattern {
              .Text              = Canonicalize(Line),
              .Raw               = Line,
              .MatchBasenameOnly = !HasSlash,
            });
        }

        return Patterns;
    }

    /// @brief True if CanonicalPath matches any ignore pattern - a
    /// no-'/' pattern (e.g. "*.psd", "thumbs.db") is checked against just
    /// the filename so it applies at any depth; any other pattern is
    /// checked against the full path, root-anchored (there's no per-
    /// directory .pakignore, so "anchored" just means "relative to the
    /// content root" here).
    bool IsIgnored(const std::string& CanonicalPath,
                   const std::vector<IgnorePattern>& Patterns,
                   std::string* OutMatchedRaw) {
        std::string_view Basename = CanonicalPath;
        if (const size_t Slash = CanonicalPath.find_last_of('/'); Slash != std::string::npos) {
            Basename = std::string_view(CanonicalPath).substr(Slash + 1);
        }

        for (const IgnorePattern& Pattern : Patterns) {
            const bool Matched =
              Pattern.MatchBasenameOnly ? GlobMatch(Basename, Pattern.Text) : GlobMatch(CanonicalPath, Pattern.Text);
            if (Matched) {
                if (OutMatchedRaw) *OutMatchedRaw = Pattern.Raw;
                return true;
            }
        }

        return false;
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
            THROW_ENGINE_EXCEPTION(EngineException, "asset count mismatch");
        }

        for (const auto& Asset : SourceAssets) {
            if (!Source.Contains(Asset.ID)) {
                THROW_ENGINE_EXCEPTION(EngineException, "asset id not found in pak: " + Asset.CanonicalPath);
            }

            AssetBuffer Loaded             = Source.LoadFull(Asset.ID);
            const std::vector<u8> Original = ReadWholeFile(Asset.AbsolutePath);

            if (Loaded.Size() != Original.size()) {
                THROW_ENGINE_EXCEPTION(EngineException, "size mismatch for " + Asset.CanonicalPath);
            }

            if (Loaded.Size() > 0 && std::memcmp(Loaded.Data(), Original.data(), Loaded.Size()) != 0) {
                THROW_ENGINE_EXCEPTION(EngineException, "byte mismatch for " + Asset.CanonicalPath);
            }
        }

        std::printf("[PAKTool] self-verify OK: %llu assets, decompressed bytes match source exactly\n",
                    CAST<u64>(SourceAssets.size()));
    }

    int RunPack(const fs::path& ContentDir,
                const fs::path& OutputPath,
                const std::optional<fs::path>& PakIgnore,
                const bool Encrypt,
                const bool Metadata) {
        std::vector<std::string> IgnorePatterns;
        if (PakIgnore.has_value()) {
            std::ifstream IgnoreFile(*PakIgnore);
            if (!IgnoreFile) {
                std::fprintf(stderr,
                             "[PAKTool] error: failed to open provided .pakignore (%s)\n",
                             PakIgnore->string().c_str());
                return EXIT_FAILURE;
            }

            std::ostringstream IgnoreStream;
            IgnoreStream << IgnoreFile.rdbuf();

            std::stringstream LineStream(IgnoreStream.str());
            std::string Line;

            while (std::getline(LineStream, Line, '\n')) {
                IgnorePatterns.push_back(Line);
            }
        }

        const std::vector<IgnorePattern> ParsedIgnorePatterns = ParseIgnorePatterns(IgnorePatterns);
        if (!ParsedIgnorePatterns.empty()) {
            std::printf("[PAKTool] Using ignore patterns: [");
            for (const auto& Pattern : ParsedIgnorePatterns) {
                std::printf("'%s'", Pattern.Raw.c_str());
                if (&Pattern != &ParsedIgnorePatterns.back()) { std::printf(", "); }
            }
            std::printf("]\n");
        }

        std::vector<ScannedAsset> Assets = ScanContentDirectory(ContentDir, CollisionPolicy::Throw);
        if (Assets.empty()) {
            std::fprintf(stderr, "[PAKTool] error: no assets found\n");
            return EXIT_FAILURE;
        }

        // Collision detection above runs over every file on disk, ignored
        // ones included - filtering first would avoid hashing/ID work for
        // files that'll just be dropped, but ContentScanner has no ignore
        // hook of its own, and this tool is the only thing that needs one.
        //
        // Deliberately not re-checking Assets.empty() after this: a content
        // directory that's non-empty before filtering but ends up with
        // nothing left after it (e.g. a placeholder-only directory whose
        // only files are .pakignore'd .keep markers) is a legitimate,
        // expected outcome, not a foot-gun - it produces a valid pak with a
        // 0-entry table. Only an empty scan (the check above) means the
        // caller likely pointed this at the wrong/an empty directory.
        if (!ParsedIgnorePatterns.empty()) {
            std::erase_if(Assets, [&](const ScannedAsset& Asset) {
                std::string MatchedPattern;
                if (!IsIgnored(Asset.CanonicalPath, ParsedIgnorePatterns, &MatchedPattern)) return false;
                std::printf("[PAKTool] ignored '%s' (matched pattern '%s')\n",
                            Asset.CanonicalPath.c_str(),
                            MatchedPattern.c_str());
                return true;
            });
        }

        std::ofstream Out(OutputPath, std::ios::binary | std::ios::trunc);
        if (!Out) {
            std::fprintf(stderr, "[PAKTool] error: failed to open output file '%s'\n", OutputPath.string().c_str());
            return EXIT_FAILURE;
        }

        // Reserve space for the header now; the real table offset/entry count aren't known until
        // every asset has been written, so this gets overwritten in place later.
        PakHeader::WriteEmpty(Out);

        // Salt/KeyCheck stay zeroed and unused when Encrypt is off - nothing
        // to derive a nonce from or check a key against.
        const PakSalt Salt                = Encrypt ? GenerateSalt() : PakSalt {};
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

            // Compress first, THEN encrypt (skipped entirely without --encrypt).
            std::vector<u8> DataToWrite = Plain;
            if (Encrypt) { AesCtrXcryptInPlace(DataToWrite, KeySchedule, DeriveNonce(ID.Value, Salt, KeySchedule)); }

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
            ManifestEntry.Encrypted        = Encrypt;
            Manifest.Assets.push_back(std::move(ManifestEntry));

            TotalUncompressed += Original.size();
            TotalStored += DataToWrite.size();
            if (!UseCompression) ++StoredRawCount;

            std::printf("[PAKTool] packed '%s' %zu -> %zu bytes (%s%s)\n",
                        CanonicalPath.c_str(),
                        Original.size(),
                        DataToWrite.size(),
                        UseCompression ? "lz4" : "stored",
                        Encrypt ? "+aes" : "");
        }

        u64 TableOffset = Out.tellp();
        for (const auto& Entry : Table) {
            Entry.Write(Out);
        }

        PakHeader Header;
        Header.TableOffset     = TableOffset;
        Header.TableEntryCount = CAST<u32>(Table.size());
        Header.Encrypted       = Encrypt;
        Header.Salt            = Salt;
        Header.KeyCheck        = Encrypt ? ComputeKeyCheck(Salt, KeySchedule) : PakKeyCheck {};
        Out.seekp(0, std::ios::beg);
        Header.Write(Out);

        Out.close();

        const fs::path ManifestPath = PakManifest::ManifestPathFor(OutputPath);
        if (Metadata) Manifest.WriteToFile(ManifestPath);

        const f64 Ratio =
          TotalUncompressed > 0 ? 100.0 * (1.0 - CAST<f64>(TotalStored) / CAST<f64>(TotalUncompressed)) : 0.0;

        std::printf("[PAKTool] wrote '%s' (%zu assets, %zu stored raw, %llu -> %llu bytes, "
                    "%.1f%% smaller, %s)\n",
                    OutputPath.string().c_str(),
                    Table.size(),
                    StoredRawCount,
                    CAST<u64>(TotalUncompressed),
                    CAST<u64>(TotalStored),
                    Ratio,
                    Encrypt ? "encrypted" : "not encrypted");
        if (Metadata) std::printf("[PAKTool] wrote manifest '%s'\n", ManifestPath.string().c_str());

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
                THROW_ENGINE_EXCEPTION(EngineException,
                                       "manifest references asset not present in pak: " + Asset.CanonicalPath);
            }

            AssetBuffer Buffer = Source.LoadFull(ID);

            const fs::path OutPath = OutputDir / fs::path(Asset.CanonicalPath);
            fs::create_directories(OutPath.parent_path());

            std::ofstream OutFile(OutPath, std::ios::binary | std::ios::trunc);
            if (!OutFile) {
                THROW_ENGINE_EXCEPTION(EngineException, "failed to open output file: " + OutPath.string());
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
    fs::path PackOutput = "Data.pxk";
    std::optional<fs::path> PakIgnore;
    bool PackEncrypt  = false;
    bool PackMetadata = false;

    CLI::App* PackCmd = App.add_subcommand("pack", "Pack a content directory into a .pxk file");
    PackCmd->add_option("content-dir", PackContentDir, "Root content directory to pack")
      ->required()
      ->check(CLI::ExistingDirectory);
    PackCmd->add_option("-o,--output",
                        PackOutput,
                        "Output .pxk filename (default: Data.pxk). Must use .pxk extension.");
    PackCmd
      ->add_option("-i,--ignore",
                   PakIgnore,
                   "Optional .pakignore file used to filter which files get packed. One glob pattern per "
                   "line ('*' = any run of characters, '?' = exactly one); '#' starts a comment. A pattern "
                   "with no '/' matches by filename at any depth (e.g. '*.psd', 'thumbs.db'); a pattern "
                   "with a '/' matches the full path from the content root (e.g. 'textures/temp/*').")
      ->check(CLI::ExistingFile);
    PackCmd->add_flag("-e,--encrypt",
                      PackEncrypt,
                      "AES-256-CTR encrypt every packed asset. Off by default - encryption adds real time to "
                      "both packing and load, which mostly buys nothing during development; enable it for a "
                      "build you're distributing.");
    PackCmd->add_flag("-m,--metadata", PackMetadata, "Optional .pxkm file containing pack content metadata.");

    fs::path UnpackPak;
    fs::path UnpackOutputDir;
    CLI::App* UnpackCmd = App.add_subcommand("unpack", "Unpack all assets from a .pxk file");
    UnpackCmd->add_option("pak-file", UnpackPak, "Pak file to unpack")->required()->check(CLI::ExistingFile);
    UnpackCmd->add_option("output-dir", UnpackOutputDir, "Directory to write unpacked assets to")->required();

    fs::path InfoPak;
    CLI::App* InfoCmd = App.add_subcommand("info", "Print detailed information about a .pxk file");
    InfoCmd->add_option("pak-file", InfoPak, "Pak file to inspect")->required()->check(CLI::ExistingFile);

    CLI11_PARSE(App, argc, argv);

    try {
        if (*PackCmd) return RunPack(PackContentDir, PackOutput, PakIgnore, PackEncrypt, PackMetadata);
        if (*UnpackCmd) return RunUnpack(UnpackPak, UnpackOutputDir);
        if (*InfoCmd) return RunInfo(InfoPak);
    } catch (const EngineException& Ex) {
        std::fprintf(stderr, "[PAKTool] error: %s\n", Ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
