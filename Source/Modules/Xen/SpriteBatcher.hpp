//
// Created by Jake Rieger on 9/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Components/CameraComponent.hpp"
#include "Components/SpriteComponent.hpp"
#include "TextureCache.hpp"

#include <vector>

namespace Xen {
    struct SpriteDrawItem {
        u64 SortKey {0};
        TextureHandle Texture {};
        Transform WorldTransform {};
        Rect SourceRect {};
        Float4 Tint {1.0f, 1.0f, 1.0f, 1.0f};
        i32 Layer {0};
    };

    struct SpriteBatch {
        TextureHandle Texture {};
        size_t FirstItem {0};
        size_t ItemCount {0};
    };

    class SpriteBatcher {
    public:
        void BuildDrawList(const Scene& S);

        NODISCARD CameraComponent* GetActiveCamera() const { return _ActiveCamera; }
        NODISCARD const Float4x4& GetViewProjection() const { return _ViewProjection; }
        NODISCARD size_t GetCulledCount() const { return _CulledCount; }

        void SetViewport(const u32 Width, const u32 Height) {
            _ViewportWidth  = Width;
            _ViewportHeight = Height;
        }

        const std::vector<SpriteDrawItem>& GetItems() const;
        const std::vector<SpriteBatch>& GetBatches() const;

        size_t GetDrawCallCount() const;

    private:
        static u64 MakeSortKey(i32 Layer, TextureHandle Texture);

        static CameraComponent* FindActiveCamera(const Scene& S);
        static Rect ComputeSpriteBounds(const Transform& WorldTransform, const Rect& SourceRect, f32 PixelsPerUnit);

        std::vector<SpriteDrawItem> _Items;
        std::vector<SpriteBatch> _Batches;

        CameraComponent* _ActiveCamera {nullptr};
        Float4x4 _ViewProjection {IdentityFloat4x4};
        u32 _ViewportWidth {1280};
        u32 _ViewportHeight {720};
        size_t _CulledCount {0};
    };
}  // namespace Xen
