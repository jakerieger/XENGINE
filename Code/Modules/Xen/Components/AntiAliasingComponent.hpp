//
// Created by Jake Rieger on 9/23/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "FXAA.hpp"
#include "TAA.hpp"

namespace Xen {
    /// @brief Which anti-aliasing pass (if any) the scene wants - the
    /// "Technique" choice this component's own doc comment used to say
    /// would land here once a second technique existed (see TAA.hpp).
    /// Mutually exclusive, not both at once: TAA already resolves both
    /// geometric and shading aliasing on its own (see TAA.hpp), so running
    /// FXAA on top of it would just soften an already-antialiased frame for
    /// no benefit.
    enum class AntiAliasingTechnique : u8 { None, FXAA, TAA };

    REGISTER_COMPONENT(AntiAliasingComponent)

    /// @brief The scene's anti-aliasing settings. Scene-wide, not per-actor -
    /// Game/MeshRenderer use the first one they find, same rule as
    /// PostProcessComponent/EnvironmentComponent/DirectionalLightComponent.
    /// A scene with none renders with Technique's own default (TAA).
    class AntiAliasingComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(AntiAliasingComponent)
        AntiAliasingComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD AntiAliasingTechnique GetTechnique() const { return _Technique; }
        void SetTechnique(const AntiAliasingTechnique Technique) { _Technique = Technique; }

        NODISCARD const FXAA::Settings& GetFxaaSettings() const { return _FxaaSettings; }
        NODISCARD FXAA::Settings& GetFxaaSettings() { return _FxaaSettings; }
        void SetFxaaSettings(const FXAA::Settings& Settings) { _FxaaSettings = Settings; }

        NODISCARD const TAA::Settings& GetTaaSettings() const { return _TaaSettings; }
        NODISCARD TAA::Settings& GetTaaSettings() { return _TaaSettings; }
        void SetTaaSettings(const TAA::Settings& Settings) { _TaaSettings = Settings; }

    private:
        AntiAliasingTechnique _Technique {AntiAliasingTechnique::TAA};
        FXAA::Settings _FxaaSettings {};
        TAA::Settings _TaaSettings {};
    };
}  // namespace Xen
