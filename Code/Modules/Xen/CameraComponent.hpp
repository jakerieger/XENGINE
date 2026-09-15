//
// Created by Jake Rieger on 9/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "Transform.hpp"

namespace Xen {
    /// @brief Defines the view into the world.
    ///
    /// A component rather than a standalone object so it inherits an actor's
    /// transform: "camera follows the player" is just attaching it to the
    /// player, and shake, parenting and interpolation all come free from the
    /// existing hierarchy.
    ///
    /// A scene may hold several (main view, minimap, split screen). The
    /// renderer draws through the enabled one with the highest Priority.
    _DefineComponent(CameraComponent);
    class CameraComponent final : public IComponent {
    public:
        _ComponentType(CameraComponent);
        CameraComponent() = default;

        void Reflect(IReflector& R) override {
            R.Property("Zoom",
                       _Zoom,
                       {.Tooltip  = "Magnification. 2 shows half as much world, twice as large.",
                        .Category = "Camera",
                        .Min      = 0.01f,
                        .Max      = 100.0f});
            R.Property("PixelsPerUnit",
                       _PixelsPerUnit,
                       {.Tooltip = "Screen pixels per world unit at zoom 1.", .Category = "Camera"});
            R.Property("Priority",
                       _Priority,
                       {.Tooltip = "Highest priority enabled camera renders the scene.", .Category = "Camera"});
        }

        void SetViewport(const u32 Width, const u32 Height) {
            _ViewportWidth  = Width;
            _ViewportHeight = Height;
        }

        _NoDiscard u32 GetViewportWidth() const { return _ViewportWidth; }
        _NoDiscard u32 GetViewportHeight() const { return _ViewportHeight; }

        _NoDiscard f32 GetZoom() const { return _Zoom; }
        void SetZoom(const f32 Z) { _Zoom = std::max<f32>(Z, 0.0001f); }

        _NoDiscard f32 GetPixelsPerUnit() const { return _PixelsPerUnit; }
        void SetPixelsPerUnit(const f32 P) { _PixelsPerUnit = std::max<f32>(P, 0.0001f); }

        _NoDiscard i32 GetPriority() const { return _Priority; }
        void SetPriority(const i32 Priority) { _Priority = Priority; }

        _NoDiscard glm::mat4 GetViewMatrix() const;
        _NoDiscard glm::mat4 GetProjectionMatrix() const;
        _NoDiscard glm::mat4 GetViewProjectionMatrix() const { return GetProjectionMatrix() * GetViewMatrix(); }

        _NoDiscard glm::vec2 GetVisibleWorldSize() const;
        _NoDiscard Rect GetViewBounds() const;
        _NoDiscard glm::vec2 ScreenToWorld(glm::vec2 ScreenPos) const;
        _NoDiscard glm::vec2 WorldToScreen(glm::vec2 WorldPos) const;

    private:
        _NoDiscard Transform GetCameraTransform() const;

        f32 _Zoom {1.0f};
        f32 _PixelsPerUnit {100.0f};
        i32 _Priority {0};

        // Runtime only
        u32 _ViewportWidth {1280};
        u32 _ViewportHeight {720};
    };
}  // namespace Xen
