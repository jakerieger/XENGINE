//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "TextureCache.hpp"

namespace Xen {
    REGISTER_COMPONENT(SpriteComponent)

      class SpriteComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(SpriteComponent) SpriteComponent();
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

        /// @brief World-space axis-aligned bounds of the sprite (position/rotation-free
        /// extent), sized from the source rect (or full texture, if unset) scaled by the
        /// actor's world transform and the scene's main camera's pixels-per-unit.
        ///
        /// For gameplay use (collision, hit-testing) - rendering's own culling bounds live
        /// separately in SpriteBatcher, which additionally inflates them for rotation.
        NODISCARD Rect GetWorldBounds() const;

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
