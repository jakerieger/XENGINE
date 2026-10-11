//
// Created by Jake Rieger on 9/24/2026.
//

#include "PointLightComponent.hpp"

namespace Xen {
    void PointLightComponent::Reflect(IReflector& R) {
        R.Property("Color", _Color, {.Category = "Light"});
        R.Property("Intensity", _Intensity, {.Category = "Light", .Min = 0.0f});
        R.Property("Range",
                   _Range,
                   {.ToolTip  = "World-space distance at which the light's contribution reaches zero.",
                    .Category = "Light",
                    .Min      = 0.01f});
    }
}  // namespace Xen
