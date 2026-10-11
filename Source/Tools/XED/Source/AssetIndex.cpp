//
// Created by Jake Rieger on 10/6/2026.
//

#include "AssetIndex.hpp"

#include <XenPAK/Canonicalize.hpp>
#include <XenPAK/ContentScanner.hpp>

#include <algorithm>
#include <cctype>

namespace Xen {
    AssetKind AssetKindFromExtension(const fs::path& File) {
        std::string Ext = File.extension().string();
        std::ranges::transform(Ext, Ext.begin(), [](const unsigned char C) { return CAST<char>(std::tolower(C)); });

        if (Ext == ".gltf" || Ext == ".glb") return AssetKind::Mesh;
        if (Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg" || Ext == ".tga" || Ext == ".bmp" || Ext == ".hdr") {
            return AssetKind::Texture;
        }
        if (Ext == ".xscene") return AssetKind::Scene;
        if (Ext == ".xprefab") return AssetKind::Prefab;
        if (Ext == ".wav" || Ext == ".ogg" || Ext == ".mp3") return AssetKind::Audio;
        return AssetKind::Unknown;
    }

    const char* AssetKindName(const AssetKind Kind) {
        switch (Kind) {
            case AssetKind::Texture: return "Texture";
            case AssetKind::Audio:   return "Audio";
            case AssetKind::Scene:   return "Scene";
            case AssetKind::Prefab:  return "Prefab";
            case AssetKind::Mesh:    return "Mesh";
            case AssetKind::Data:    return "Data";
            case AssetKind::Unknown: break;
        }
        return "Asset";
    }

    AssetID AssetIDFromRelativePath(const fs::path& RelativePath) {
        return PAK::HashPath(PAK::Canonicalize(RelativePath.generic_string()));
    }

    void AssetIndex::Rescan(const fs::path& Root) {
        _Root = Root;
        _Entries.clear();
        _ByID.clear();

        if (Root.empty() || !fs::is_directory(Root)) return;

        for (const PAK::ScannedAsset& Scanned : PAK::ScanContentDirectory(Root, PAK::CollisionPolicy::WarnAndKeepFirst)) {
            AssetEntry Entry;
            Entry.ID           = Scanned.ID;
            Entry.RelativePath = fs::relative(Scanned.AbsolutePath, Root);
            Entry.Name         = Scanned.AbsolutePath.filename().string();
            Entry.Kind         = AssetKindFromExtension(Scanned.AbsolutePath);

            _ByID.emplace(Entry.ID, _Entries.size());
            _Entries.push_back(std::move(Entry));
        }
    }

    const AssetEntry* AssetIndex::Find(const AssetID ID) const {
        const auto It = _ByID.find(ID);
        return It == _ByID.end() ? nullptr : &_Entries[It->second];
    }
}  // namespace Xen
