//
// Created by Jake Rieger on 9/23/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "SSAO.hpp"

namespace Xen {
    REGISTER_COMPONENT(AmbientOcclusionComponent)

    /// @brief The scene's screen-space ambient occlusion settings (see
    /// SSAO). Scene-wide, not per-actor - MeshRenderer uses the first one it
    /// finds, same rule as PostProcessComponent/AntiAliasingComponent. A
    /// scene with none renders with SSAO::Settings's defaults (on).
    class AmbientOcclusionComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(AmbientOcclusionComponent)
        AmbientOcclusionComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const SSAO::Settings& GetSettings() const { return _Settings; }
        NODISCARD SSAO::Settings& GetSettings() { return _Settings; }
        void SetSettings(const SSAO::Settings& Settings) { _Settings = Settings; }

    private:
        SSAO::Settings _Settings {};
    };
}  // namespace Xen
