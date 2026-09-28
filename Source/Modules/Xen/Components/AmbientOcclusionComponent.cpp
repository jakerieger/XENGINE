//
// Created by Jake Rieger on 9/23/2026.
//

#include "AmbientOcclusionComponent.hpp"

namespace Xen {
    void AmbientOcclusionComponent::Reflect(IReflector& R) {
        R.Property("Enabled",
                   _Settings.Enabled,
                   {.ToolTip = "SSAO - darkens contact/crevice areas' ambient lighting based on nearby geometry.",
                    .Category = "Ambient Occlusion"});
        R.Property("Radius",
                   _Settings.Radius,
                   {.ToolTip  = "World-space distance something can still occlude a surface from.",
                    .Category = "Ambient Occlusion",
                    .Min      = 0.01f,
                    .Max      = 10.0f});
        R.Property("Power",
                   _Settings.Power,
                   {.ToolTip  = "Darkens and sharpens the effect above 1; 1 is the raw physical result.",
                    .Category = "Ambient Occlusion",
                    .Min      = 0.1f,
                    .Max      = 8.0f});
        R.Property("Bias",
                   _Settings.Bias,
                   {.ToolTip  = "World-space bias against self-occlusion acne - raise it if flat surfaces darken.",
                    .Category = "Ambient Occlusion",
                    .Min      = 0.0f,
                    .Max      = 1.0f});
    }
}  // namespace Xen
