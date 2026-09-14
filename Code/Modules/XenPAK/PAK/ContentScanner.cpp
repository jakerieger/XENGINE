//
// Created by Jake Rieger on 9/7/2026.
//

#include "ContentScanner.hpp"
#include "Canonicalize.hpp"

#include <stdexcept>
#include <unordered_map>

namespace Xen::PAK {
    namespace fs = std::filesystem;

    std::vector<ScannedAsset> ScanContentDirectory(const fs::path& RootDir,
                                                   CollisionPolicy Policy) {
        if (!exists(RootDir)) {
            throw std::runtime_error("ScanContentDirectory - root directory does not exist: " +
                                     RootDir.string());
        }

        std::vector<ScannedAsset> Results;
        std::unordered_map<AssetIDValue, size_t> IndexByID;

        for (const auto& Entry : fs::recursive_directory_iterator(RootDir)) {
            if (!Entry.is_regular_file()) continue;

            fs::path Relative     = fs::relative(Entry.path(), RootDir);
            std::string Canonical = Canonicalize(Relative.generic_string());
            AssetID ID            = HashPath(Canonical);

            if (auto It = IndexByID.find(ID.Value); It != IndexByID.end()) {
                const ScannedAsset& Existing = Results[It->second];
                if (Existing.AbsolutePath == Entry.path()) {
                    // Same file visited twice (e.g. a symlink loop) - skip quietly
                    continue;
                }

                std::string Message = std::format("Hash collision  between '{}' and '{}' (id={})",
                                                  Existing.AbsolutePath.string(),
                                                  Entry.path().string(),
                                                  ID.Value);
                if (Policy == CollisionPolicy::Throw) { throw std::runtime_error(Message); }
                std::fprintf(stderr, "[ContentScanner] %s\n", Message.c_str());

                continue;
            }

            IndexByID[ID.Value] = Results.size();
            Results.push_back(ScannedAsset {
              .ID            = ID,
              .AbsolutePath  = Entry.path(),
              .CanonicalPath = Canonical,
            });
        }

        return Results;
    }
}  // namespace Xen::PAK