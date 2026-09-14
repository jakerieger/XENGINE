//
// Created by Jake Rieger on 9/9/2026.
//

#include "SpriteBatcher.hpp"
#include "Actor.hpp"
#include "Scene.hpp"

#include <algorithm>

namespace Xen {
    void SpriteBatcher::BuildDrawList(const Scene& S) {
        // clear() keeps capacity, so a steady-state frame does no allocation.
        _Items.clear();
        _Batches.clear();
        _CulledCount = 0;

        const TextureCache* Textures = S.GetContext().Textures;

        _ActiveCamera = FindActiveCamera(S);

        // Viewport lives on the window, so push the current size onto whichever
        // camera is active this frame rather than expecting scenes to carry it.
        bool Cull = false;
        Rect ViewBounds {};
        f32 PixelsPerUnit = 100.0f;

        if (_ActiveCamera) {
            _ActiveCamera->SetViewport(_ViewportWidth, _ViewportHeight);
            _ViewProjection = _ActiveCamera->GetViewProjectionMatrix();
            ViewBounds      = _ActiveCamera->GetViewBounds();
            PixelsPerUnit   = _ActiveCamera->GetPixelsPerUnit();
            Cull            = true;
        } else {
            // No camera: draw everything unculled rather than nothing, so a
            // scene missing a camera shows up as "wrong view" rather than a
            // blank screen with no clue why.
            _ViewProjection = glm::mat4 {1.0f};
        }

        S.ForEachActor([&](Actor& A) {
            if (!A.IsEnabled()) return;

            // GetComponents, not GetComponent: an actor may carry several
            // sprites on different layers (a body plus an overlay).
            for (const SpriteComponent* Sprite : A.GetComponents<SpriteComponent>()) {
                if (!Sprite->IsEnabled() || !Sprite->IsVisible()) continue;

                const TextureHandle Tex = Sprite->GetTexture();
                if (!Tex.IsValid()) continue;  // BeginPlay hasn't run yet

                SpriteDrawItem Item;
                Item.Texture        = Tex;
                Item.WorldTransform = A.GetWorldTransform();
                Item.Tint           = Sprite->GetTint();
                Item.Layer          = Sprite->GetLayer();

                // Resolve an empty rect to the full texture here, so the
                // backend always receives concrete pixel bounds and never has
                // to special-case the standalone-texture path.
                Item.SourceRect = Sprite->GetSourceRect();
                if (Item.SourceRect.IsEmpty() && Textures) {
                    const auto [Width, Height] = Textures->GetInfo(Tex);
                    Item.SourceRect            = Rect {0.0f, 0.0f, CAST<f32>(Width), CAST<f32>(Height)};
                }

                if (Cull) {
                    const Rect Bounds = ComputeSpriteBounds(Item.WorldTransform, Item.SourceRect, PixelsPerUnit);
                    if (!Bounds.Intersects(ViewBounds)) {
                        ++_CulledCount;
                        continue;
                    }
                }

                Item.SortKey = MakeSortKey(Item.Layer, Tex);
                _Items.push_back(Item);
            }
        });

        // Stable so sprites sharing a layer and texture keep scene order.
        // Without stability their relative order could shift frame to frame,
        // which shows up as flicker on overlapping sprites.
        std::ranges::stable_sort(_Items, [](const SpriteDrawItem& A, const SpriteDrawItem& B) {
            return A.SortKey < B.SortKey;
        });

        // The sort grouped identical textures together, so starting a new
        // batch on every texture change yields the minimum batch count for
        // this ordering.
        for (size_t i = 0; i < _Items.size(); ++i) {
            if (!_Batches.empty() && _Batches.back().Texture == _Items[i].Texture) {
                ++_Batches.back().ItemCount;
                continue;
            }
            _Batches.push_back(SpriteBatch {_Items[i].Texture, i, 1});
        }
    }

    const std::vector<SpriteDrawItem>& SpriteBatcher::GetItems() const {
        return _Items;
    }

    const std::vector<SpriteBatch>& SpriteBatcher::GetBatches() const {
        return _Batches;
    }

    size_t SpriteBatcher::GetDrawCallCount() const {
        return _Batches.size();
    }

    u64 SpriteBatcher::MakeSortKey(const i32 Layer, const TextureHandle Texture) {
        const u64 BiasedLayer = CAST<u64>(CAST<i64>(Layer) + (CAST<i64>(1) << 31));
        return (BiasedLayer << 32) | CAST<u64>(Texture.ID);
    }

    CameraComponent* SpriteBatcher::FindActiveCamera(const Scene& S) {
        CameraComponent* Best = nullptr;

        S.ForEachActor([&](const Actor& A) {
            if (!A.IsEnabled()) return;
            for (CameraComponent* C : A.GetComponents<CameraComponent>()) {
                if (!C->IsEnabled()) continue;
                if (!Best || C->GetPriority() > Best->GetPriority()) Best = C;
            }
        });

        return Best;
    }

    Rect SpriteBatcher::ComputeSpriteBounds(const Transform& WorldTransform,
                                            const Rect& SourceRect,
                                            const f32 PixelsPerUnit) {
        const glm::vec2 Size {SourceRect.Width / PixelsPerUnit * std::abs(WorldTransform.Scale.x),
                              SourceRect.Height / PixelsPerUnit * std::abs(WorldTransform.Scale.y)};

        const f32 Extent = WorldTransform.Rotation == 0.0f ? 0.0f : std::max(Size.x, Size.y) * 0.41422f;  // sqrt(2)-1

        const glm::vec2 Half {Size.x * 0.5f + Extent, Size.y * 0.5f + Extent};

        return Rect {WorldTransform.Position.x - Half.x,
                     WorldTransform.Position.y - Half.y,
                     Half.x * 2.0f,
                     Half.y * 2.0f};
    }
}  // namespace Xen