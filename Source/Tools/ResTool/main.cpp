//
// Created by Jake Rieger on 9/29/2026.
//
// Currently only supports embedding image files. Other file types coming soon

#include <Common/Brotli.hpp>
#include <CLI/CLI.hpp>
#include <stb_image.h>

namespace {
    using namespace Xen;

    /// @brief How many bytes to put on each line within the byte array.
    constexpr auto ARRAY_LINE_WIDTH = 12;

    struct SymbolNames {
        std::string Identifier;
        std::string Bytes;
        std::string OriginalSize;
        std::string CompressedSize;
    };

    bool GetSymbolNames(const std::filesystem::path& Path, SymbolNames& Symbols) {
        if (!exists(Path)) return false;
        auto Identifier = Path.filename().has_stem() ? Path.filename().stem().string() : Path.filename().string();
        std::ranges::transform(Identifier, Identifier.begin(), ::toupper);  // Force uppercase

        Symbols.Identifier     = Identifier;
        Symbols.Bytes          = Identifier + "_" + "BYTES";
        Symbols.OriginalSize   = Identifier + "_" + "ORIGINAL_SIZE";
        Symbols.CompressedSize = Identifier + "_" + "COMPRESSED_SIZE";

        return true;
    }

    std::optional<ByteArray> CompressImage(
      const std::filesystem::path& ImagePath, size_t& OriginalSize, i32& Width, i32& Height, i32& Channels) {
        if (!exists(ImagePath)) return {};

        stbi_uc* Data = stbi_load(ImagePath.string().c_str(), &Width, &Height, &Channels, 4);
        if (!Data) return {};

        OriginalSize = Width * Height * Channels;
        ByteArray OriginalBytes(OriginalSize);
        std::copy_n(Data, OriginalSize, OriginalBytes.begin());
        stbi_image_free(Data);

        const auto CompressResult = Brotli::Compress(OriginalBytes);
        if (!CompressResult.has_value()) return OriginalBytes;

        if (CompressResult->size() < OriginalSize) return *CompressResult;
        return OriginalBytes;
    }

    bool IsImageFile(const std::filesystem::path& Path) {
        if (!exists(Path)) return false;
        if (!is_regular_file(Path)) return false;
        const auto Ext = Path.extension().string();
        return Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg";
    }
}  // namespace

int main(const int argc, char* argv[]) {
    CLI::App ResTool {"ResTool - Utility for compressing and embedding application resources in C header files."};

    std::filesystem::path OutputPath;
    std::vector<std::filesystem::path> InputFiles;

    ResTool.add_option("-o,--output", OutputPath, "Output header file path")->required();
    ResTool.add_option("-i,--input", InputFiles, "Input file path(s)")
      ->required()
      ->check(CLI::ExistingFile)
      ->expected(1, -1);

    try {
        ResTool.parse(argc, argv);
    } catch (const CLI::ParseError& Ex) {
        std::fprintf(stderr, "error parsing args: %s\n", Ex.what());
        return EXIT_FAILURE;
    }

    std::ofstream O(OutputPath, std::ios::out);
    if (!O.is_open()) {
        std::fprintf(stderr, "error: failed to open output header file: '%s'\n", OutputPath.string().c_str());
        return EXIT_FAILURE;
    }

    O << "/// Generated with ResTool - DO NOT MODIFY.\n\n";
    O << "#pragma once\n\n";
    O << R""(extern "C" {)"";
    O << "\n";

    for (auto It = InputFiles.begin(); It != InputFiles.end(); ++It) {
        if (!IsImageFile(*It)) continue;

        SymbolNames Symbols;
        if (!GetSymbolNames(*It, Symbols)) continue;

        std::printf("- Packing '%s'\n", It->string().c_str());

        size_t OriginalSize;
        i32 Width, Height, Channels;
        const auto CompressResult = CompressImage(*It, OriginalSize, Width, Height, Channels);
        if (!CompressResult.has_value()) {
            std::printf("- Failed to compress '%s', skipping it\n", It->string().c_str());
            continue;
        }
        std::printf("- Compressed '%s'\n", It->string().c_str());

        std::ostringstream B;
        B << "    //=========================================================================================//\n";
        B << "    // " + It->filename().string() + "\n";
        B << "    // Width: " + std::to_string(Width) + ", Height: " + std::to_string(Height) + "\n";
        B << "    //=========================================================================================//\n";
        B << "    static const unsigned char " + Symbols.Bytes + "[" + std::to_string(CompressResult->size()) +
               "] = {\n        ";

        size_t BytesRemaining = CompressResult->size();
        size_t Offset         = 0;

        while (BytesRemaining > 0) {
            size_t BytesToRead = (BytesRemaining < ARRAY_LINE_WIDTH) ? BytesRemaining : ARRAY_LINE_WIDTH;
            for (size_t i = 0; i < BytesToRead; i++) {
                B << "0x" << std::setfill('0') << std::setw(2) << std::hex << CAST<i32>((*CompressResult)[Offset + i]);

                if (Offset + i + 1 < CompressResult->size()) {
                    B << ", ";
                    if ((Offset + i + 1) % ARRAY_LINE_WIDTH == 0) { B << "\n        "; }
                }
            }

            Offset += BytesToRead;
            BytesRemaining -= BytesToRead;
        }

        B << "\n    };\n\n";
        B << "    static constexpr size_t " + Symbols.OriginalSize + " = " + std::to_string(OriginalSize) + ";\n";
        B << "    static constexpr size_t " + Symbols.CompressedSize + " = " + std::to_string(CompressResult->size()) +
               ";\n";
        B << "    static constexpr int " + Symbols.Identifier + "_WIDTH = " + std::to_string(Width) + ";\n";
        B << "    static constexpr int " + Symbols.Identifier + "_HEIGHT = " + std::to_string(Height) + ";\n";
        B << "    //=========================================================================================//\n";

        if (std::next(It) != InputFiles.end()) { B << "\n"; }

        O << B.str();
    }

    O << "}\n";

    std::printf("- Finished generating embedding application resources -> '%s'\n",
                canonical(OutputPath).string().c_str());

    return 0;
}