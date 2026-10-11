//
// Created by ${USER} on ${DATE}
//

#pragma once

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>

XEN_COMPONENT(${COMPONENT_CLASS});
class ${COMPONENT_CLASS} final : public Xen::IComponent {
public:
    XEN_COMPONENT_STATICS(${COMPONENT_CLASS});

    // Components must be default constructable
    ${COMPONENT_CLASS}() = default;
    
    void Reflect(Xen::IReflector& R) override;

    void BeginPlay() override;
    void Tick(Xen::f32 DeltaTime) override;
    void EndPlay() override;
};
