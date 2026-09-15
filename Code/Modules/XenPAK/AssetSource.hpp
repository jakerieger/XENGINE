//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include "AssetID.hpp"
#include "AssetBuffer.hpp"

namespace Xen::PAK {
    /// @brief Common interface for anything that can resolve an AssetID to bytes.
    class IAssetSource {
    public:
        virtual ~IAssetSource() = default;

        /// @brief Fast existence check.
        virtual bool Contains(AssetID ID) const = 0;

        /// @brief Fully load and return the asset's bytes. Only valid to call if Contains(ID) is
        /// true.
        virtual AssetBuffer LoadFull(AssetID ID) = 0;

        /// @brief Sources are checked in descending priority order by AssetRegistry.
        virtual int Priority() const = 0;

        // @brief For logging/debugging only.
        virtual const char* DebugName() const = 0;
    };
}  // namespace Xen::PAK