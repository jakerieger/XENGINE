//
// Created by Jake Rieger on 9/22/2026.
//

#include "PostProcessComponent.hpp"

namespace Xen {
    void PostProcessComponent::Reflect(IReflector& R) {
        R.Property("Exposure",
                   _Settings.Exposure,
                   {.ToolTip = "Manual exposure, used only while AutoExposureEnabled is off. 1 is neutral.",
                    .Category = "Exposure",
                    .Min      = 0.01f,
                    .Max      = 16.0f});
        R.Property("AutoExposureEnabled",
                   _Settings.AutoExposureEnabled,
                   {.ToolTip  = "Meters the scene's own brightness each frame and derives exposure from it, "
                                "instead of using the fixed Exposure above.",
                    .Category = "Exposure"});
        R.Property("AutoExposureKey",
                   _Settings.AutoExposureKey,
                   {.ToolTip = "The metered average luminance is scaled to land here - 0.18 is \"18% middle gray\".",
                    .Category = "Exposure",
                    .Min      = 0.01f,
                    .Max      = 1.0f});
        R.Property("AutoExposureMinLuminance",
                   _Settings.AutoExposureMinLuminance,
                   {.ToolTip  = "Lower clamp on the metered luminance auto exposure reacts to.",
                    .Category = "Exposure",
                    .Min      = 0.0001f,
                    .Max      = 10.0f});
        R.Property("AutoExposureMaxLuminance",
                   _Settings.AutoExposureMaxLuminance,
                   {.ToolTip  = "Upper clamp on the metered luminance auto exposure reacts to.",
                    .Category = "Exposure",
                    .Min      = 0.1f,
                    .Max      = 1000.0f});
        R.Property("AutoExposureAdaptUpSeconds",
                   _Settings.AutoExposureAdaptUpSeconds,
                   {.ToolTip  = "Time (seconds) for exposure to catch up when the scene gets brighter.",
                    .Category = "Exposure",
                    .Min      = 0.01f,
                    .Max      = 10.0f});
        R.Property("AutoExposureAdaptDownSeconds",
                   _Settings.AutoExposureAdaptDownSeconds,
                   {.ToolTip  = "Time (seconds) for exposure to catch up when the scene gets darker.",
                    .Category = "Exposure",
                    .Min      = 0.01f,
                    .Max      = 10.0f});
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
