//
// Created by Jake Rieger on 9/23/2026.
//

#include "AntiAliasingComponent.hpp"

namespace Xen {
    void AntiAliasingComponent::Reflect(IReflector& R) {
        R.EnumProperty("Technique",
                       _Technique,
                       {.ToolTip  = "None, FXAA (spatial only) or TAA (temporal - the fuller fix, on by default).",
                        .Category = "Anti-Aliasing"});
        R.Property("TAA.BlendFactor",
                   _TaaSettings.BlendFactor,
                   {.ToolTip  = "How much of each new frame to blend into the running history - lower is more "
                                "stable but ghosts longer after a disocclusion.",
                    .Category = "Anti-Aliasing",
                    .Min      = 0.01f,
                    .Max      = 1.0f});
    }
}  // namespace Xen
