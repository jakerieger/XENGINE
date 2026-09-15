//
// Created by Jake Rieger on 9/14/2026.
//

#include "PakManifest.hpp"

#include <Common/Log.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

namespace Xen::PAK {
    using Json = nlohmann::json;

    namespace {
        const char* CodecToString(const PakCodec Codec) {
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

        PakCodec CodecFromString(const std::string& Str) {
            if (Str == "ZStd") return PakCodec::ZStd;
            if (Str == "Lz4") return PakCodec::Lz4;
            return PakCodec::None;
        }
    }  // namespace

    void PakManifest::WriteToFile(const std::filesystem::path& Path) const {
        Json AssetArray = Json::array();
        for (const auto& Asset : Assets) {
            AssetArray.push_back(Json {
              {"id", Asset.ID},
              {"canonical_path", Asset.CanonicalPath},
              {"uncompressed_size", Asset.UncompressedSize},
              {"compressed_size", Asset.CompressedSize},
              {"codec", CodecToString(Asset.Codec)},
              {"compressed", Asset.Compressed},
              {"encrypted", Asset.Encrypted},
            });
        }

        const Json Root {
          {"format_version", FormatVersion},
          {"pak_file", PakFilename},
          {"assets", AssetArray},
        };

        std::ofstream Out(Path, std::ios::trunc);
        if (!Out) {
            _ThrowEngineException(InvalidManifestException, "failed to open manifest for writing: " + Path.string());
        }
        Out << Root.dump(2);
    }

    PakManifest PakManifest::ReadFromFile(const std::filesystem::path& Path) {
        std::ifstream In(Path);
        if (!In) { _ThrowEngineException(InvalidManifestException, "manifest not found: " + Path.string()); }

        Json Root;
        try {
            In >> Root;
        } catch (const Json::parse_error& Ex) {
            _ThrowEngineException(InvalidManifestException,
                                  "failed to parse manifest '" + Path.string() + "': " + Ex.what());
        }

        PakManifest Manifest;
        try {
            Manifest.FormatVersion = Root.at("format_version").get<u32>();
            Manifest.PakFilename   = Root.at("pak_file").get<std::string>();

            for (const auto& Entry : Root.at("assets")) {
                PakManifestEntry AssetEntry;
                AssetEntry.ID               = Entry.at("id").get<AssetIDValue>();
                AssetEntry.CanonicalPath    = Entry.at("canonical_path").get<std::string>();
                AssetEntry.UncompressedSize = Entry.at("uncompressed_size").get<u32>();
                AssetEntry.CompressedSize   = Entry.at("compressed_size").get<u32>();
                AssetEntry.Codec            = CodecFromString(Entry.at("codec").get<std::string>());
                AssetEntry.Compressed       = Entry.at("compressed").get<bool>();
                AssetEntry.Encrypted        = Entry.at("encrypted").get<bool>();
                Manifest.Assets.push_back(std::move(AssetEntry));
            }
        } catch (const Json::exception& Ex) {
            _ThrowEngineException(InvalidManifestException, "malformed manifest '" + Path.string() + "': " + Ex.what());
        }

        if (Manifest.FormatVersion != PAK_MANIFEST_VERSION) {
            _ThrowEngineException(InvalidManifestException,
                                  "unsupported manifest version: " + std::to_string(Manifest.FormatVersion));
        }

        return Manifest;
    }

    std::filesystem::path PakManifest::ManifestPathFor(const std::filesystem::path& PakPath) {
        std::filesystem::path Result = PakPath;
        Result.replace_extension(".xmeta");
        return Result;
    }
}  // namespace Xen::PAK
