//
// Created by Jake Rieger on 9/7/2026.
//

#include "AssetMount.hpp"
#include "LooseFileSource.hpp"
#include "PakFileSource.hpp"

#include <Common/Log.hpp>

namespace Xen::PAK {
    namespace fs = std::filesystem;

    std::unique_ptr<AssetRegistry> MountAssets(const AssetMountConfig& Config) {
        if (Config.PakFiles.empty() && Config.ContentDirs.empty()) {
            THROW_ENGINE_EXCEPTION(MountException, "no pak files or content directories were configured");
        }

        auto Registry = std::make_unique<AssetRegistry>();

        int Priority = MOUNT_PRIORITY_PAK_BASE;
        for (const fs::path& PakPath : Config.PakFiles) {
            if (!exists(PakPath)) {
                THROW_ENGINE_EXCEPTION(MountException, "pak file does not exist: " + PakPath.string());
            }

            Registry->AddSource(std::make_unique<PakFileSource>(PakPath, Priority));
            Priority += MOUNT_PRIORITY_STEP;
        }

        Priority = MOUNT_PRIORITY_LOOSE_BASE;
        for (const fs::path& Dir : Config.ContentDirs) {
            if (!exists(Dir)) {
                THROW_ENGINE_EXCEPTION(MountException, "content directory does not exist: " + Dir.string());
            }

            if (!is_directory(Dir)) {
                THROW_ENGINE_EXCEPTION(MountException, "content path is not a directory: " + Dir.string());
            }

            Registry->AddSource(std::make_unique<LooseFileSource>(Dir, Priority));
            Priority += MOUNT_PRIORITY_STEP;
        }

        return Registry;
    }

    void AppendContentDirsFromArgs(AssetMountConfig& Config, const int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            if (const std::string Arg = argv[i]; Arg != "--content-dir") continue;
            if (i + 1 >= argc) { THROW_ENGINE_EXCEPTION(MountException, "--content-dir requires a path argument"); }
            Config.ContentDirs.emplace_back(argv[++i]);
        }
    }

    std::string DescribeMounts(const AssetRegistry& Registry) {
        std::string Out = std::format("{} source(s), highest priority first:\n", Registry.SourceCount());

        for (size_t i = 0; i < Registry.SourceCount(); ++i) {
            const IAssetSource* Source = Registry.SourceAt(i);

            std::string Path;
            if (Source->GetTypeID() == PakFileSource::StaticTypeID()) {
                const PakFileSource* PakSource = Source->As<PakFileSource>();
                Path                           = PakSource->Path().string();
            }
            if (Source->GetTypeID() == LooseFileSource::StaticTypeID()) {
                const LooseFileSource* LooseSource = Source->As<LooseFileSource>();
                Path                               = LooseSource->RootDir().string();
            }

            Out += std::format("  [{}] {} (priority {}) -> '{}'\n", i, Source->GetTypeName(), Source->Priority(), Path);
        }

        return Out;
    }
}  // namespace Xen::PAK