//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Engine/ComponentRegistry.hpp>

_DefineComponent(OpponentComponent)

class OpponentComponent final : public Xen::IComponent {
public:
    _ComponentType(OpponentComponent)
    OpponentComponent() {}

    void Reflect(Xen::IReflector& R) override {}
    void BeginPlay() override {}
    void Tick(Xen::f32 DeltaTime) override {}
    void EndPlay() override {}
};