//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include "SpriteComponent.hpp"
#include "Actor.hpp"
#include "CameraComponent.hpp"
#include "Scene.hpp"

#include <cmath>

namespace Xen {
    SpriteComponent::SpriteComponent() = default;

    SpriteComponent::SpriteComponent(const AssetID Texture, const Rect SourceRect)
        : _TextureAsset(Texture), _SourceRect(SourceRect) {
        if (!_TextureAsset.IsValid()) { THROW_ENGINE_EXCEPTION(EngineException, "invalid sprite asset ID"); }
    }

    void SpriteComponent::Reflect(IReflector& R) {
        R.Property("TextureAsset", _TextureAsset, {.Category = "Sprite", .Asset = AssetKind::Texture});
        R.Property("SourceRect",
                   _SourceRect,
                   {.ToolTip = "Region of the texture to draw. Empty means the whole texture.", .Category = "Sprite"});
        R.Property("Layer", _Layer, {.ToolTip = "Draw order. Lower layers draw first.", .Category = "Sprite"});
        R.Property("Tint", _Tint, {.Category = "Sprite"});
        R.Property("Visible", _Visible, {.Category = "Sprite"});
        // _Texture is deliberately not reflected: it is a runtime GPU
        // handle, meaningless across sessions, and rebuilt in BeginPlay.
    }

    void SpriteComponent::Tick(const f32 DeltaTime) {}

    AssetID SpriteComponent::GetTextureAsset() const {
        return _TextureAsset;
    }

    const Rect& SpriteComponent::GetSourceRect() const {
        return _SourceRect;
    }

    void SpriteComponent::SetSourceRect(const Rect& NewSourceRect) {
        _SourceRect = NewSourceRect;
    }

    i32 SpriteComponent::GetLayer() const {
        return _Layer;
    }

    void SpriteComponent::SetLayer(const i32 NewLayer) {
        _Layer = NewLayer;
    }

    const Float4& SpriteComponent::GetTint() const {
        return _Tint;
    }

    void SpriteComponent::SetTint(const Float4& NewTint) {
        _Tint = NewTint;
    }

    bool SpriteComponent::IsVisible() const {
        return _Visible;
    }

    void SpriteComponent::SetVisible(const bool NewVisible) {
        _Visible = NewVisible;
    }

    TextureHandle SpriteComponent::GetTexture() const {
        return _Texture;
    }

    Rect SpriteComponent::GetWorldBounds() const {
        Rect Source = _SourceRect;
        if (Source.IsEmpty()) {
            if (const Scene* S = GetScene(); S && S->GetContext().Textures && _Texture.IsValid()) {
                const TextureInfo Info = S->GetContext().Textures->GetInfo(_Texture);
                Source                 = Rect {0.0f, 0.0f, CAST<f32>(Info.Width), CAST<f32>(Info.Height)};
            }
        }

        f32 PixelsPerUnit = 100.0f;
        if (const Scene* S = GetScene()) {
            if (const CameraComponent* Camera = S->GetMainCamera()) { PixelsPerUnit = Camera->GetPixelsPerUnit(); }
        }

        const Transform WorldTransform = GetOwner()->GetWorldTransform();
        const Float2 Size {Source.Width / PixelsPerUnit * std::abs(WorldTransform.Scale.x),
                          Source.Height / PixelsPerUnit * std::abs(WorldTransform.Scale.y)};

        return Rect {WorldTransform.Position.x - Size.x * 0.5f,
                    WorldTransform.Position.y - Size.y * 0.5f,
                    Size.x,
                    Size.y};
    }

    void SpriteComponent::BeginPlay() {
        if (!_TextureAsset.IsValid()) {
            THROW_ENGINE_EXCEPTION(EngineException,
                                  std::format("SpriteComponent on actor '{}' has no texture asset assigned",
                                              GetOwner() ? GetOwner()->GetName() : "<none>"));
        }

        const Scene* S = GetScene();
        if (!S || !S->GetContext().Textures) {
            THROW_ENGINE_EXCEPTION(EngineException,
                                  "SpriteComponent requires a scene with a TextureCache in its EngineContext");
        }

        _Texture  = S->GetContext().Textures->Acquire(_TextureAsset);
        _Acquired = true;
    }

    void SpriteComponent::EndPlay() {
        if (!_Acquired) return;

        if (const Scene* S = GetScene(); S && S->GetContext().Textures) {
            S->GetContext().Textures->Release(_TextureAsset);
        }

        _Acquired = false;
        _Texture  = {};
    }

    void SpriteComponent::SetTextureAsset(const AssetID ID) {
        if (ID == _TextureAsset) return;

        if (!_Acquired) {
            _TextureAsset = ID;
            return;
        }

        const Scene* S      = GetScene();
        TextureCache* Cache = S ? S->GetContext().Textures : nullptr;
        if (!Cache) {
            _TextureAsset = ID;
            return;
        }

        const TextureHandle New = Cache->Acquire(ID);
        Cache->Release(_TextureAsset);

        _TextureAsset = ID;
        _Texture      = New;
    }
}  // namespace Xen