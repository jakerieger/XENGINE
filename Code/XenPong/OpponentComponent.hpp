//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/ComponentRegistry.hpp>

REGISTER_COMPONENT(OpponentComponent)

class OpponentComponent final : public Xen::IComponent {
public:
    XEN_COMPONENT_TYPE(OpponentComponent)
    OpponentComponent() {}

    void Reflect(Xen::IReflector& R) override {}
    void BeginPlay() override {}
    void Tick(Xen::f32 DeltaTime) override {}
    void EndPlay() override {}
};