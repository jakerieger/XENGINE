//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    REGISTER_COMPONENT(RotatingComponent)

    /// @brief Spins the owning actor continuously - just enough motion to
    /// see the PBR shading change across a mesh's surface as it turns,
    /// without needing any input handling.
    class RotatingComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(RotatingComponent)
        RotatingComponent() = default;

        void Reflect(IReflector& R) override;
        void Tick(f32 DeltaTime) override;

    private:
        f32 _DegreesPerSecond {45.0f};
        f32 _AccumulatedRadians {0.0f};
    };
}  // namespace Xen
