//
// Created by Jake Rieger on 9/6/2026.
//

#include "LooseFileSource.hpp"
#include "Canonicalize.hpp"

#include <Common/Log.hpp>
#include <Common/Exception.hpp>
#include <fstream>
#include <cstdio>

namespace Xen::PAK {
    namespace fs = std::filesystem;

    LooseFileSource::LooseFileSource(fs::path RootDir, const int Priority)
        : _RootDir(std::move(RootDir)), _Priority(Priority) {
        ScanDirectory();
    }

    bool LooseFileSource::Contains(const AssetID ID) const {
        return _PathMap.contains(ID.Value);
    }

    AssetBuffer LooseFileSource::LoadFull(const AssetID ID) {
        const auto It = _PathMap.find(ID.Value);
        if (It == _PathMap.end()) {
            THROW_ENGINE_EXCEPTION(EngineException, "LooseFileSource::LoadFull - Unknown asset ID.");
        }

        const fs::path& Path = It->second;

        std::ifstream FileStream(Path, std::ios::binary | std::ios::ate);
        if (!FileStream) {
            THROW_ENGINE_EXCEPTION(EngineException, "LooseFileSource::LoadFull - Could not open file: " + Path.string());
        }

        const std::streamsize Size = FileStream.tellg();
        if (Size < 0) {
            THROW_ENGINE_EXCEPTION(EngineException, "LooseFileSource::LoadFull - tellg failed: " + Path.string());
        }
        FileStream.seekg(0, std::ios::beg);

        auto Data = std::make_unique<u8[]>(CAST<size_t>(Size));
        if (Size > 0 && !FileStream.read(RCAST<char*>(Data.get()), Size)) {
            THROW_ENGINE_EXCEPTION(EngineException, "LooseFileSource::LoadFull - Could not read file: " + Path.string());
        }

        return AssetBuffer(std::move(Data), CAST<size_t>(Size));
    }

    int LooseFileSource::Priority() const {
        return _Priority;
    }

    std::string LooseFileSource::DebugPathFor(const AssetID ID) const {
        const auto It = _PathMap.find(ID.Value);
        return It != _PathMap.end() ? It->second.string() : std::string();
    }

    void LooseFileSource::ScanDirectory() {
        if (!exists(_RootDir)) {
            std::fprintf(stderr, "[LooseFileSource] Root directory does not exist: %s\n", _RootDir.string().c_str());
            return;
        }

        for (const auto& Entry : fs::recursive_directory_iterator(_RootDir)) {
            if (!Entry.is_regular_file()) continue;

            fs::path Relative     = fs::relative(Entry.path(), _RootDir);
            std::string Canonical = Canonicalize(Relative.generic_string());
            AssetID ID            = HashPath(Canonical);

            if (auto Existing = _PathMap.find(ID.Value);
                Existing != _PathMap.end() && Existing->second != Entry.path()) {
                std::fprintf(stderr,
                             "[LooseFileSource] Hash collision between '%s' and '%s' (ID=%llu) - "
                             "keeping first file",
                             Existing->second.string().c_str(),
                             Canonical.c_str(),
                             ID.Value);
                continue;
            }

            _PathMap[ID.Value] = Entry.path();
        }
    }
}  // namespace Xen::PAK