//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include <Xen/EngineCommon.hpp>

#include <memory>

namespace Xen::PAK {
    /// @brief Owning buffer for fully-loaded asset data.
    class AssetBuffer {
    public:
        AssetBuffer() = default;

        AssetBuffer(std::unique_ptr<u8[]> Data, const size_t Size) : _Data(std::move(Data)), _Size(Size) {}

        AssetBuffer(AssetBuffer&&)                 = default;
        AssetBuffer& operator=(AssetBuffer&&)      = default;
        AssetBuffer(const AssetBuffer&)            = delete;
        AssetBuffer& operator=(const AssetBuffer&) = delete;

        u8* Data() { return _Data.get(); }
        [[nodiscard]] const u8* Data() const { return _Data.get(); }
        [[nodiscard]] size_t Size() const { return _Size; }
        [[nodiscard]] bool IsValid() const { return _Data != nullptr && _Size > 0; }

    private:
        std::unique_ptr<u8[]> _Data;
        size_t _Size {0};
    };
}  // namespace Xen::PAK