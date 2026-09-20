//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetSource.hpp"
#include "PakFormat.hpp"
#include "Crypto.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace Xen::PAK {
    class PakFileSource : public IAssetSource {
    public:
        _AssetSourceType(PakFileSource);

        PakFileSource(std::filesystem::path PakPath, int Priority);

        size_t AssetCount() const { return _Table.size(); }
        const std::filesystem::path& Path() const { return _PakPath; }

    private:
        void OpenAndReadTable();

    public:
        bool Contains(AssetID ID) const override;
        AssetBuffer LoadFull(AssetID ID) override;
        int Priority() const override;

    private:
        std::filesystem::path _PakPath;
        int _Priority;
        std::ifstream _FileStream;

        // LoadFull is called from AssetLoader's worker threads as well as the
        // main thread, and they'd otherwise race on _FileStream's single
        // shared seek position. Held only around the seek+read; decrypt and
        // decompress run outside it, so workers still unpack in parallel.
        std::mutex _ReadMutex;

        std::unordered_map<AssetIDValue, PakTableEntry> _Table;
        PakSalt _Salt {};
    };
}  // namespace Xen::PAK