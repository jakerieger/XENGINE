//
// Created by Jake Rieger on 9/23/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "FXAA.hpp"

namespace Xen {
    REGISTER_COMPONENT(AntiAliasingComponent)

    /// @brief The scene's anti-aliasing settings (see FXAA). Scene-wide, not
    /// per-actor - Game uses the first one it finds, same rule as
    /// PostProcessComponent/EnvironmentComponent/DirectionalLightComponent.
    /// A scene with none renders with FXAA::Settings's defaults (on).
    ///
    /// Deliberately just a single flag for now, not a whole family of
    /// tuning knobs the way PostProcessComponent has for bloom - the
    /// underlying algorithm (Code/Shaders/FXAA.hlsl) doesn't expose any;
    /// this is where a "Technique" choice (FXAA/TAA/off) would live once a
    /// second one exists.
    class AntiAliasingComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(AntiAliasingComponent)
        AntiAliasingComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const FXAA::Settings& GetSettings() const { return _Settings; }
        NODISCARD FXAA::Settings& GetSettings() { return _Settings; }
        void SetSettings(const FXAA::Settings& Settings) { _Settings = Settings; }

    private:
        FXAA::Settings _Settings {};
    };
}  // namespace Xen
