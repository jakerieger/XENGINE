//
// Created by Jake Rieger on 9/16/2026.
//

#pragma once

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    REGISTER_COMPONENT(GameManagerComponent);
    class GameManagerComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(GameManagerComponent);
        GameManagerComponent() = default;

        void Reflect(IReflector& R) override;

    private:
        u32 _PlayerScore {0};
        u32 _OpponentScore {0};
        u32 _ScoreToWin {5};
    };
}  // namespace Xen
