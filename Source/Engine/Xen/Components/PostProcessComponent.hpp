//
// Created by Jake Rieger on 9/22/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>
#include <Xen/PostProcess.hpp>

namespace Xen {
    XEN_COMPONENT(PostProcessComponent)

    /// @brief The scene's exposure and bloom settings (see PostProcess).
    ///
    /// Scene-wide, not per-actor - MeshRenderer uses the first one it finds,
    /// same rule as EnvironmentComponent and DirectionalLightComponent. A
    /// scene with none renders with PostProcess::Settings's defaults.
    class PostProcessComponent final : public IComponent {
    public:
        XEN_COMPONENT_STATICS(PostProcessComponent)
        PostProcessComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const PostProcess::Settings& GetSettings() const { return _Settings; }
        NODISCARD PostProcess::Settings& GetSettings() { return _Settings; }
        void SetSettings(const PostProcess::Settings& Settings) { _Settings = Settings; }

    private:
        PostProcess::Settings _Settings {};
    };
}  // namespace Xen
