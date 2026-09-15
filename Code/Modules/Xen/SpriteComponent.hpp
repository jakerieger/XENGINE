//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "TextureCache.hpp"

namespace Xen {
    _DefineComponent(SpriteComponent)

      class SpriteComponent final : public IComponent {
    public:
        _ComponentType(SpriteComponent) SpriteComponent();
        explicit SpriteComponent(AssetID Texture, Rect SourceRect = {});

        void Reflect(IReflector& R) override;

        void Tick(f32 DeltaTime) override;

        void BeginPlay() override;
        void EndPlay() override;

        AssetID GetTextureAsset() const;

        /// @brief Swaps the texture at runtime, releasing the old one and
        /// acquiring the new so reference counts stay balanced.
        void SetTextureAsset(AssetID ID);

        const Rect& GetSourceRect() const;
        void SetSourceRect(const Rect& NewSourceRect);

        i32 GetLayer() const;
        void SetLayer(i32 NewLayer);

        const glm::vec4& GetTint() const;
        void SetTint(const glm::vec4& NewTint);

        bool IsVisible() const;
        void SetVisible(bool NewVisible);

        /// @brief The resolved GPU texture. Invalid until BeginPlay has run.
        TextureHandle GetTexture() const;

    private:
        AssetID _TextureAsset {};
        Rect _SourceRect {};
        i32 _Layer {0};
        glm::vec4 _Tint {1.0f};
        bool _Visible {true};

        // Runtime state
        TextureHandle _Texture {};
        bool _Acquired {false};
    };
}  // namespace Xen
