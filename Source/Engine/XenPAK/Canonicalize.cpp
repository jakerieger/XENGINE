//
// Created by Jake Rieger on 9/6/2026.
//

#include "Canonicalize.hpp"

#include <Common/XenCommon.hpp>
#include <algorithm>
#include <cctype>

namespace Xen::PAK {
    std::string Canonicalize(const std::string& Path) {
        std::string OutPath = Path;

        // Normalize separators
        std::ranges::replace(OutPath, '\\', '/');
        // ASCII lowercase. Asset paths are expected to be ASCII.
        std::ranges::transform(OutPath, OutPath.begin(), [](const unsigned char c) {
            return CAST<char>(std::tolower(c));
        });

        // Strip leading "./"
        while (OutPath.size() >= 2 && OutPath[0] == '.' && OutPath[1] == '/') {
            OutPath.erase(0, 2);
        }

        // Strip leading slash(es)
        const size_t FirstNonSlash = OutPath.find_first_not_of('/');
        if (FirstNonSlash == std::string::npos) return "";
        if (FirstNonSlash > 0) OutPath.erase(0, FirstNonSlash);

        // Collapse duplicate slashes
        std::string Result;
        Result.reserve(OutPath.size());
        bool LastWasSlash = false;
        for (const char c : OutPath) {
            if (c == '/') {
                if (LastWasSlash) continue;
                LastWasSlash = true;
            } else {
                LastWasSlash = false;
            }
            Result.push_back(c);
        }

        // Strip trailing slash
        while (!Result.empty() && Result.back() == '/')
            Result.pop_back();

        return Result;
    }
}  // namespace Xen::PAK