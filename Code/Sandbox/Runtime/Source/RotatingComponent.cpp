//
// Created by Jake Rieger on 9/17/2026.
//

#include "RotatingComponent.hpp"
#include <Xen/Actor.hpp>

namespace Xen {
    void RotatingComponent::Reflect(IReflector& R) {
        R.Property("DegreesPerSecond", _DegreesPerSecond, {.Category = "Rotating"});
    }

    void RotatingComponent::Tick(const f32 DeltaTime) {
        using namespace DirectX;

        _AccumulatedRadians += XMConvertToRadians(_DegreesPerSecond) * DeltaTime;

        if (Actor* Owner = GetOwner()) {
            const XMVECTOR Rotation = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), _AccumulatedRadians);
            Quat Out;
            XMStoreFloat4(&Out, Rotation);
            Owner->SetRotation(Out);
        }
    }
}  // namespace Xen
