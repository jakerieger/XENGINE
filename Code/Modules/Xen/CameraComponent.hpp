//
// Created by Jake Rieger on 9/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "Transform.hpp"

#include <algorithm>

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
    /// @brief Orthographic (2D, the only mode that existed before) or
    /// perspective (3D). Kept on the one CameraComponent rather than as a
    /// separate type - same reasoning as Transform growing to 3D in place:
    /// one camera concept a scene can mix (an ortho HUD camera, a
    /// perspective scene camera) rather than two parallel object models.
    enum class ProjectionMode : u8 { Orthographic, Perspective };

    REGISTER_COMPONENT(CameraComponent);
    class CameraComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(CameraComponent);
        CameraComponent() = default;

        void Reflect(IReflector& R) override {
            R.EnumProperty("ProjectionMode",
                           _ProjectionMode,
                           {.ToolTip = "Orthographic (2D) or perspective (3D).", .Category = "Camera"});
            R.Property("Zoom",
                       _Zoom,
                       {.ToolTip  = "Orthographic only. Magnification: 2 shows half as much world, twice as large.",
                        .Category = "Camera",
                        .Min      = 0.01f,
                        .Max      = 100.0f});
            R.Property("PixelsPerUnit",
                       _PixelsPerUnit,
                       {.ToolTip  = "Orthographic only. Screen pixels per world unit at zoom 1.",
                        .Category = "Camera"});
            R.Property("FieldOfView",
                       _FieldOfViewDegrees,
                       {.ToolTip  = "Perspective only. Vertical field of view, in degrees.",
                        .Category = "Camera",
                        .Min      = 1.0f,
                        .Max      = 179.0f});
            R.Property("NearPlane",
                       _NearPlane,
                       {.ToolTip = "Perspective only. Distance to the near clip plane.", .Category = "Camera"});
            R.Property("FarPlane",
                       _FarPlane,
                       {.ToolTip = "Perspective only. Distance to the far clip plane.", .Category = "Camera"});
            R.Property("Priority",
                       _Priority,
                       {.ToolTip = "Highest priority enabled camera renders the scene.", .Category = "Camera"});
        }

        void SetViewport(const u32 Width, const u32 Height) {
            _ViewportWidth  = Width;
            _ViewportHeight = Height;
        }

        NODISCARD u32 GetViewportWidth() const { return _ViewportWidth; }
        NODISCARD u32 GetViewportHeight() const { return _ViewportHeight; }

        NODISCARD ProjectionMode GetProjectionMode() const { return _ProjectionMode; }
        void SetProjectionMode(const ProjectionMode Mode) { _ProjectionMode = Mode; }

        NODISCARD f32 GetZoom() const { return _Zoom; }
        void SetZoom(const f32 Z) { _Zoom = std::max<f32>(Z, 0.0001f); }

        NODISCARD f32 GetPixelsPerUnit() const { return _PixelsPerUnit; }
        void SetPixelsPerUnit(const f32 P) { _PixelsPerUnit = std::max<f32>(P, 0.0001f); }

        NODISCARD f32 GetFieldOfView() const { return _FieldOfViewDegrees; }
        void SetFieldOfView(const f32 Degrees) { _FieldOfViewDegrees = std::clamp(Degrees, 1.0f, 179.0f); }

        NODISCARD f32 GetNearPlane() const { return _NearPlane; }
        void SetNearPlane(const f32 Near) { _NearPlane = std::max(Near, 0.0001f); }

        NODISCARD f32 GetFarPlane() const { return _FarPlane; }
        void SetFarPlane(const f32 Far) { _FarPlane = std::max(Far, _NearPlane + 0.0001f); }

        NODISCARD f32 GetAspectRatio() const {
            return _ViewportHeight > 0 ? CAST<f32>(_ViewportWidth) / CAST<f32>(_ViewportHeight) : 1.0f;
        }

        NODISCARD i32 GetPriority() const { return _Priority; }
        void SetPriority(const i32 Priority) { _Priority = Priority; }

        NODISCARD Float4x4 GetViewMatrix() const;
        NODISCARD Float4x4 GetProjectionMatrix() const;

        /// @brief View-then-projection, composed for DirectXMath's row-vector
        /// convention (v' = v * View * Projection).
        NODISCARD Float4x4 GetViewProjectionMatrix() const {
            using namespace DirectX;
            const Float4x4 View = GetViewMatrix();
            const Float4x4 Proj = GetProjectionMatrix();
            const XMMATRIX M    = XMLoadFloat4x4(&View) * XMLoadFloat4x4(&Proj);

            Float4x4 Out;
            XMStoreFloat4x4(&Out, M);
            return Out;
        }

        NODISCARD Float2 GetVisibleWorldSize() const;
        NODISCARD Rect GetViewBounds() const;
        NODISCARD Float2 ScreenToWorld(Float2 ScreenPos) const;
        NODISCARD Float2 WorldToScreen(Float2 WorldPos) const;

    private:
        NODISCARD Transform GetCameraTransform() const;

        ProjectionMode _ProjectionMode {ProjectionMode::Orthographic};
        f32 _Zoom {1.0f};
        f32 _PixelsPerUnit {100.0f};
        f32 _FieldOfViewDegrees {60.0f};
        f32 _NearPlane {0.1f};
        f32 _FarPlane {1000.0f};
        i32 _Priority {0};

        // Runtime only
        u32 _ViewportWidth {1280};
        u32 _ViewportHeight {720};
    };
}  // namespace Xen
