//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/ComponentRegistry.hpp>

_DefineComponent(PlayerComponent)

class PlayerComponent final : public Xen::IComponent {
public:
    _ComponentType(PlayerComponent)
    PlayerComponent() {}

    void Reflect(Xen::IReflector& R) override;
    void BeginPlay() override;
    void Tick(Xen::f32 DeltaTime) override;
    void FixedTick(Xen::f32 FixedDelta) override;
    void EndPlay() override;

    void Reset();
};