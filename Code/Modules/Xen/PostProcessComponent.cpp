//
// Created by Jake Rieger on 9/22/2026.
//

#include "PostProcessComponent.hpp"

namespace Xen {
    void PostProcessComponent::Reflect(IReflector& R) {
        R.Property("Exposure",
                   _Settings.Exposure,
                   {.ToolTip = "Multiplies scene brightness before tonemapping. 1 is neutral.",
                    .Category = "Exposure",
                    .Min      = 0.01f,
                    .Max      = 16.0f});
        R.Property("BloomEnabled", _Settings.BloomEnabled, {.Category = "Bloom"});
        R.Property("BloomThreshold",
                   _Settings.BloomThreshold,
                   {.ToolTip  = "A pixel's brightest channel must exceed this to bloom.",
                    .Category = "Bloom",
                    .Min      = 0.0f});
        R.Property("BloomSoftKnee",
                   _Settings.BloomSoftKnee,
                   {.ToolTip  = "0 = hard cutoff at Threshold; higher fades bloom in gradually below it.",
                    .Category = "Bloom",
                    .Min      = 0.0f,
                    .Max      = 1.0f});
        R.Property("BloomIntensity",
                   _Settings.BloomIntensity,
                   {.ToolTip = "How much of the blurred highlights are added back into the scene.",
                    .Category = "Bloom",
                    .Min      = 0.0f,
                    .Max      = 1.0f});
    }
}  // namespace Xen
