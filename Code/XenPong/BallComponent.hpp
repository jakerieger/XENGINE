//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/Actor.hpp>
#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>
#include <../Modules/Common/XenCommon.hpp>
#include <random>

_DefineComponent(BallComponent)

  class BallComponent final : public Xen::IComponent {
public:
    _ComponentType(BallComponent);

    BallComponent();

    void Reflect(Xen::IReflector& R) override;
    void BeginPlay() override;
    void Tick(Xen::f32 DeltaTime) override;
    void FixedTick(Xen::f32 FixedDelta) override;
    void EndPlay() override;

    void Reset();

private:
    glm::vec2 _Velocity {0.0f, 0.0f};
    Xen::f32 _BallSpeed {5.0f};
    glm::vec2 _Bounds {0.0f, 0.0f};
    Xen::f32 _SpeedGain {1.0f};

    std::mt19937 _Rng;
};