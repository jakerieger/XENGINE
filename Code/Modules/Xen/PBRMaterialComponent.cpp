//
// Created by Jake Rieger on 9/17/2026.
//

#include "PBRMaterialComponent.hpp"

namespace Xen {
    void PBRMaterialComponent::Reflect(IReflector& R) {
        R.Property("Albedo", _Albedo, {.Category = "Material"});
        R.Property("Metallic", _Metallic, {.Category = "Material", .Min = 0.0f, .Max = 1.0f});
        R.Property("Roughness", _Roughness, {.Category = "Material", .Min = 0.045f, .Max = 1.0f});
        R.Property("AmbientOcclusion", _AmbientOcclusion, {.Category = "Material", .Min = 0.0f, .Max = 1.0f});
        R.Property("Emissive", _Emissive, {.Category = "Material"});
    }
}  // namespace Xen
