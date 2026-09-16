//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    REGISTER_COMPONENT(PlayerComponent)

    class PlayerComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(PlayerComponent)
        PlayerComponent() {}

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void Tick(f32 DeltaTime) override;
        void FixedTick(f32 FixedDelta) override;
        void EndPlay() override;

        void Reset();

    private:
        // Cached at BeginPlay, same reasoning as BallComponent's _BallHalfSize: the
        // paddle's own SpriteComponent has already resolved its texture by then.
        f32 _HalfHeight {0.0f};
    };
}  // namespace Xen