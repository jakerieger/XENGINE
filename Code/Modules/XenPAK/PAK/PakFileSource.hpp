//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "PakCommon.hpp"
#include "AssetSource.hpp"
#include "PakFormat.hpp"
#include "Crypto.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace Xen::PAK {
    class PakFileSource : public IAssetSource {
    public:
        PakFileSource(std::filesystem::path PakPath, int Priority);

        size_t AssetCount() const { return _Table.size(); }
        const std::filesystem::path& Path() const { return _PakPath; }

    private:
        void OpenAndReadTable();

    public:
        bool Contains(AssetID ID) const override;
        AssetBuffer LoadFull(AssetID ID) override;
        int Priority() const override;
        const char* DebugName() const override;

    private:
        std::filesystem::path _PakPath;
        int _Priority;
        std::ifstream _FileStream;
        std::unordered_map<AssetIDValue, PakTableEntry> _Table;
        PakSalt _Salt {};
    };
}  // namespace Xen::PAK