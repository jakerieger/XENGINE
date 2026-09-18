//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"

#include <algorithm>

namespace Xen {
    REGISTER_COMPONENT(PBRMaterialComponent)

    /// @brief Constant-factor metallic-roughness PBR material - no textures
    /// yet, just the scalar factors a Cook-Torrance BRDF needs. A texture-
    /// mapped version (albedo/normal/metallic-roughness/AO/emissive maps)
    /// is the natural next step once this constant-factor path is proven;
    /// it would add texture AssetID fields here without disturbing these.
    class PBRMaterialComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(PBRMaterialComponent)
        PBRMaterialComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const Float3& GetAlbedo() const { return _Albedo; }
        void SetAlbedo(const Float3& Albedo) { _Albedo = Albedo; }

        NODISCARD f32 GetMetallic() const { return _Metallic; }
        void SetMetallic(const f32 Metallic) { _Metallic = std::clamp(Metallic, 0.0f, 1.0f); }

        NODISCARD f32 GetRoughness() const { return _Roughness; }
        // Never fully 0: a perfectly smooth GGX distribution divides by a
        // near-zero denominator and the specular highlight degenerates to a
        // single point that aliases badly with no shadow map to soften it.
        void SetRoughness(const f32 Roughness) { _Roughness = std::clamp(Roughness, 0.045f, 1.0f); }

        NODISCARD f32 GetAmbientOcclusion() const { return _AmbientOcclusion; }
        void SetAmbientOcclusion(const f32 AO) { _AmbientOcclusion = std::clamp(AO, 0.0f, 1.0f); }

        NODISCARD const Float3& GetEmissive() const { return _Emissive; }
        void SetEmissive(const Float3& Emissive) { _Emissive = Emissive; }

    private:
        Float3 _Albedo {1.0f, 1.0f, 1.0f};
        f32 _Metallic {0.0f};
        f32 _Roughness {0.5f};
        f32 _AmbientOcclusion {1.0f};
        Float3 _Emissive {0.0f, 0.0f, 0.0f};
    };
}  // namespace Xen
