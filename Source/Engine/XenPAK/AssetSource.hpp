//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include "AssetID.hpp"
#include "AssetBuffer.hpp"

namespace Xen::PAK {
    using AssetSourceID = u64;

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

        /// @brief Static type name/ID for runtime checks without RTTI
        virtual const char* GetTypeName() const = 0;
        virtual AssetSourceID GetTypeID() const = 0;

        template<typename T>
        const T* As() const {
            return static_cast<const T*>(const_cast<IAssetSource*>(this));
        }

        template<typename T>
        T* As() {
            return static_cast<T*>(this);
        }
    };

#define _AssetSourceType(TypeName)                                                                                     \
    static constexpr const char* StaticTypeName() {                                                                    \
        return #TypeName;                                                                                              \
    }                                                                                                                  \
    static constexpr Xen::PAK::AssetSourceID StaticTypeID() {                                                          \
        return Xen::Hash::FNV1A(#TypeName, Xen::Hash::detail::ConstexprStrLen(#TypeName));                             \
    }                                                                                                                  \
    const char* GetTypeName() const override {                                                                         \
        return StaticTypeName();                                                                                       \
    }                                                                                                                  \
    Xen::PAK::AssetSourceID GetTypeID() const override {                                                               \
        return StaticTypeID();                                                                                         \
    }
}  // namespace Xen::PAK