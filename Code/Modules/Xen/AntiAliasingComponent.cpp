//
// Created by Jake Rieger on 9/23/2026.
//

#include "AntiAliasingComponent.hpp"

namespace Xen {
    void AntiAliasingComponent::Reflect(IReflector& R) {
        R.Property("Enabled",
                   _Settings.Enabled,
                   {.ToolTip = "FXAA - smooths jagged edges and aliased highlights in the final composited frame.",
                    .Category = "Anti-Aliasing"});
    }
}  // namespace Xen
