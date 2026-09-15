//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/ComponentRegistry.hpp>

REGISTER_COMPONENT(PlayerComponent)

class PlayerComponent final : public Xen::IComponent {
public:
    XEN_COMPONENT_TYPE(PlayerComponent)
    PlayerComponent() {}

    void Reflect(Xen::IReflector& R) override;
    void BeginPlay() override;
    void Tick(Xen::f32 DeltaTime) override;
    void FixedTick(Xen::f32 FixedDelta) override;
    void EndPlay() override;

    void Reset();
};