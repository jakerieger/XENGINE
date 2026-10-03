//
// Created by Jake Rieger on 9/10/2026.
//

#pragma once

#include "Component.hpp"
#include "ComponentRegistry.hpp"

namespace Xen {
    XEN_COMPONENT(AudioSourceComponent);

    class AudioSourceComponent final : public IComponent {
    public:
        XEN_COMPONENT_STATICS(AudioSourceComponent);
    };
}  // namespace Xen
