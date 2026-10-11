//
// Created by ${USER} on ${DATE}
//

#include "${COMPONENT_CLASS}.hpp"

void ${COMPONENT_CLASS}::Reflect(Xen::IReflector& R) {
    /// Add properties to reflect here via R.Property
}

void ${COMPONENT_CLASS}::BeginPlay() {
    /// Called when actor this component is attached 
    /// to is first constructed in scene.
}

void ${COMPONENT_CLASS}::Tick(Xen::f32 DeltaTime) {
    /// Called every frame (override FixedTick for physics
    /// and other frame-rate-dependent logic)
}

void ${COMPONENT_CLASS}::EndPlay() {
    /// Called when actor this component is attached
    /// to is destroyed
}